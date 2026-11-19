
/*
 * Copyright (C) Liu Lantao
 */


#include <syslog.h>
#include "ngx_traffic_accounting.h"


/*
 * Reserved accounting ids (see README, "Security hardening"):
 *
 *   default   fallback for empty or unset ids; always tracked in its
 *             own node, even past the metrics cap
 *   overflow  ids beyond the per-period cap are accounted here
 *
 * A real id that equals a reserved word is merged into that bucket.
 * The memory bound stays max_metrics + 2 nodes.
 */
static ngx_str_t  ngx_traffic_accounting_default_name = ngx_string("default");
static ngx_str_t  ngx_traffic_accounting_overflow_name = ngx_string("overflow");

static void ngx_traffic_accounting_period_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static void ngx_traffic_accounting_period_normalize_name(ngx_str_t *out, u_char *buf,
    ngx_str_t *name);
static ngx_int_t ngx_traffic_accounting_name_eq(ngx_str_t *a, ngx_str_t *b);
static ngx_traffic_accounting_metrics_t *ngx_traffic_accounting_period_lookup_metrics(
    ngx_traffic_accounting_period_t *period, ngx_str_t *name);
static ngx_traffic_accounting_metrics_t *ngx_traffic_accounting_period_insert(
    ngx_traffic_accounting_period_t *period, ngx_str_t *name, ngx_log_t *log);
static ngx_rbtree_node_t *ngx_traffic_accounting_period_successor(
    ngx_rbtree_t *rbtree, ngx_rbtree_node_t *node);
static void ngx_traffic_accounting_period_destroy_subtree(ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel);

ngx_int_t
ngx_traffic_accounting_metrics_init(ngx_traffic_accounting_metrics_t *metrics, size_t len, ngx_log_t *log)
{
    if (metrics->nr_status == NULL) {
        metrics->nr_status = ngx_calloc(sizeof(ngx_uint_t) * len, log);

        if (metrics->nr_status == NULL)
            return NGX_ERROR;
    }

    if (metrics->nr_upstream_status == NULL) {
        metrics->nr_upstream_status = ngx_calloc(sizeof(ngx_uint_t) * len, log);

        if (metrics->nr_upstream_status == NULL)
            return NGX_ERROR;
    }

    return NGX_OK;
}

ngx_int_t
ngx_traffic_accounting_period_init(ngx_traffic_accounting_period_t *period)
{
    ngx_rbtree_init(&period->rbtree, &period->sentinel,
                    ngx_traffic_accounting_period_insert_value);

    return NGX_OK;
}

static ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_period_insert(ngx_traffic_accounting_period_t *period, ngx_str_t *name, ngx_log_t *log)
{
    ngx_traffic_accounting_metrics_t   *metrics;
    u_char                            *data;

    metrics = ngx_calloc(sizeof(ngx_traffic_accounting_metrics_t), log);
    if (metrics == NULL) {
        return NULL;
    }

    data = ngx_calloc(name->len + 1, log);
    if (data == NULL) {
        ngx_free(metrics);
        return NULL;
    }

    ngx_memcpy(data, name->data, name->len);

    metrics->name.data = data;
    metrics->name.len = name->len;

    metrics->rbnode.key = ngx_hash_key_lc(metrics->name.data, metrics->name.len);

    ngx_rbtree_insert(&period->rbtree, &metrics->rbnode);

    period->nr_metrics++;

    return metrics;
}

/*
 * Lookup mirrors the insert ordering exactly (hash key, name length,
 * memcmp).  The length comparison guarantees that memcmp only ever runs
 * on equal-length buffers, so colliding hashes can neither read past a
 * stored name nor match a different node.
 */
static ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_period_lookup_metrics(ngx_traffic_accounting_period_t *period, ngx_str_t *name)
{
    ngx_int_t                           rc;
    ngx_traffic_accounting_metrics_t   *n;
    ngx_rbtree_node_t                  *node, *sentinel;
    ngx_rbtree_key_t                    hash;

    hash = ngx_hash_key_lc(name->data, name->len);

    node = period->rbtree.root;
    sentinel = period->rbtree.sentinel;

    while (node != sentinel) {
        if (hash != node->key) {
            node = (hash < node->key) ? node->left : node->right;
            continue;
        }

        n = (ngx_traffic_accounting_metrics_t *) node;

        if (name->len != n->name.len) {
            node = (name->len < n->name.len) ? node->left : node->right;
            continue;
        }

        rc = ngx_memcmp(name->data, n->name.data, name->len);

        if (rc < 0) {
            node = node->left;
            continue;
        }

        if (rc > 0) {
            node = node->right;
            continue;
        }

        return n;
    }

    return NULL;
}

/*
 * Normalize an accounting id at the trust boundary, before it is hashed,
 * looked up or stored: truncate to NGXTA_MAX_NAME_LEN, replace characters
 * that could forge or corrupt exported log lines (control characters and
 * the field separator '|'), and map empty ids to "default" so every
 * request stays accounted.  Lookup, hash and storage all see the same
 * normalized bytes.
 */
static void
ngx_traffic_accounting_period_normalize_name(ngx_str_t *out, u_char *buf, ngx_str_t *name)
{
    size_t   i;

    out->len = ngx_min(name->len, NGXTA_MAX_NAME_LEN);

    for (i = 0; i < out->len; i++) {
        buf[i] = (name->data[i] < 0x20 || name->data[i] == 0x7f
                  || name->data[i] == '|') ? '_' : name->data[i];
    }

    out->data = buf;

    if (out->len == 0) {
        *out = ngx_traffic_accounting_default_name;
    }
}

ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_period_fetch_metrics(ngx_traffic_accounting_period_t *period, ngx_str_t *name, ngx_log_t *log)
{
    u_char                             buf[NGXTA_MAX_NAME_LEN];
    ngx_str_t                          clean;
    ngx_traffic_accounting_metrics_t  *n;

    ngx_traffic_accounting_period_normalize_name(&clean, buf, name);

    n = ngx_traffic_accounting_period_lookup_metrics(period, &clean);
    if (n != NULL) {
        return n;
    }

    /* The reserved fallback id "default" is always tracked in its own
     * bucket even past the cap, so requests with unset ids are never
     * hidden inside "overflow".  Bound stays max_metrics + 2. */
    if (period->nr_metrics >= period->max_metrics
        && !ngx_traffic_accounting_name_eq(&clean, &ngx_traffic_accounting_default_name))
    {
        if (!period->overflowed) {
            period->overflowed = 1;
            if (log != NULL) {
                ngx_log_error(NGX_LOG_ALERT, log, 0,
                              "traffic accounting: more than %ui accounting "
                              "ids in one period, new ids are accounted as "
                              "\"overflow\"", period->max_metrics);
            } else {
                syslog(LOG_ALERT,
                       "traffic accounting: more than %lu accounting ids in "
                       "one period, new ids are accounted as \"overflow\"",
                       (unsigned long) period->max_metrics);
            }
        }

        n = ngx_traffic_accounting_period_lookup_metrics(
                period, &ngx_traffic_accounting_overflow_name);
        if (n != NULL) {
            return n;
        }

        return ngx_traffic_accounting_period_insert(
                   period, &ngx_traffic_accounting_overflow_name, log);
    }

    return ngx_traffic_accounting_period_insert(period, &clean, log);
}

static ngx_int_t
ngx_traffic_accounting_name_eq(ngx_str_t *a, ngx_str_t *b)
{
    if (a->len != b->len) {
        return 0;
    }

    return a->len == 0 || ngx_memcmp(a->data, b->data, a->len) == 0;
}

/*
 * In-order successor via parent pointers.  Only the tree root has a NULL
 * parent (we never delete nodes), so the walk terminates on NULL.
 */
static ngx_rbtree_node_t *
ngx_traffic_accounting_period_successor(ngx_rbtree_t *rbtree, ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *parent;

    if (node->right != rbtree->sentinel) {
        return ngx_rbtree_min(node->right, rbtree->sentinel);
    }

    for (parent = node->parent;
         parent != NULL && node == parent->right;
         node = parent, parent = node->parent)
    {
        /* empty */
    }

    return parent;
}

/*
 * Non-destructive in-order walk.  Deletion happens wholesale in
 * ngx_traffic_accounting_period_destroy() once export is done, so no
 * per-node rbtree_delete (and no restart-from-root) is needed.
 */
ngx_int_t
ngx_traffic_accounting_period_rbtree_iterate(ngx_traffic_accounting_period_t *period,
    ngx_traffic_accounting_period_iterate_func func, void *para1, void *para2)
{
    ngx_int_t                           rc;
    ngx_rbtree_node_t                  *node, *sentinel;
    ngx_traffic_accounting_metrics_t   *n;

    sentinel = period->rbtree.sentinel;

    if (period->rbtree.root == sentinel) {
        return NGX_OK;
    }

    for (node = ngx_rbtree_min(period->rbtree.root, sentinel);
         node != NULL && node != sentinel;
         node = ngx_traffic_accounting_period_successor(&period->rbtree, node))
    {
        n = (ngx_traffic_accounting_metrics_t *) node;

        rc = func(n, para1, para2);

        if (rc != NGX_OK) {
            return rc;
        }
    }

    return NGX_OK;
}

/*
 * Post-order free of the whole tree.  Recursion depth is bounded by the
 * red-black height, 2*log2(n+1) (at most ~42 frames for 2^20 nodes).
 */
static void
ngx_traffic_accounting_period_destroy_subtree(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_traffic_accounting_metrics_t  *n;

    if (node == sentinel) {
        return;
    }

    ngx_traffic_accounting_period_destroy_subtree(node->left, sentinel);
    ngx_traffic_accounting_period_destroy_subtree(node->right, sentinel);

    n = (ngx_traffic_accounting_metrics_t *) node;

    ngx_free(n->nr_status);
    ngx_free(n->nr_upstream_status);
    ngx_free(n->name.data);
    ngx_free(n);
}

void
ngx_traffic_accounting_period_destroy(ngx_traffic_accounting_period_t *period)
{
    if (period == NULL) {
        return;
    }

    ngx_traffic_accounting_period_destroy_subtree(period->rbtree.root,
                                                  &period->sentinel);
    ngx_free(period);
}

static void
ngx_traffic_accounting_period_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_traffic_accounting_metrics_t   *n, *t; // node
    ngx_rbtree_node_t  **p;

    for ( ;; ) {
        n = (ngx_traffic_accounting_metrics_t *) node;
        t = (ngx_traffic_accounting_metrics_t *) temp;

        if (node->key != temp->key) {
            p = (node->key < temp->key) ? &temp->left : &temp->right;
        } else if (n->name.len != t->name.len) {
            p = (n->name.len < t->name.len) ? &temp->left : &temp->right;
        } else {
            p = (ngx_memcmp(n->name.data, t->name.data, n->name.len) < 0)
                 ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}
