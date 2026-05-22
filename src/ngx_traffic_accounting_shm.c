
/*
 * Copyright (C) Liu Lantao
 */


#include "ngx_traffic_accounting.h"
#include "ngx_traffic_accounting_module.h"
#include "ngx_traffic_accounting_shm.h"


static void ngx_traffic_accounting_shm_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);

static ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_shm_lookup_metrics(ngx_traffic_accounting_period_t *period,
    ngx_str_t *name);

static ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_shm_insert_metrics(ngx_traffic_accounting_period_t *period,
    ngx_str_t *name, ngx_log_t *log);


char *
ngx_traffic_accounting_set_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf,
    void *tag)
{
    ngx_traffic_accounting_main_conf_t *amcf = conf;
    ngx_str_t                          *value, name;
    size_t                              size;
    ngx_shm_zone_t                     *shm_zone;

    value = cf->args->elts;

    name = value[1];

    size = ngx_parse_size(&value[2]);
    if (size == (size_t) NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid zone size \"%V\"", &value[2]);
        return NGX_CONF_ERROR;
    }

    if (size < (size_t) (8 * ngx_pagesize)) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "zone \"%V\" is too small", &value[1]);
        return NGX_CONF_ERROR;
    }

    shm_zone = ngx_shared_memory_add(cf, &name, size, tag);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    shm_zone->init = ngx_traffic_accounting_init_zone;

    amcf->shm_zone = shm_zone;

    return NGX_CONF_OK;
}


ngx_int_t
ngx_traffic_accounting_init_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_slab_pool_t                     *shpool;
    ngx_traffic_accounting_shm_head_t   *head;
    ngx_traffic_accounting_period_t     *period;

    shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    head = ngx_slab_alloc(shpool, sizeof(ngx_traffic_accounting_shm_head_t));
    if (head == NULL) {
        return NGX_ERROR;
    }

    ngx_memzero(head, sizeof(ngx_traffic_accounting_shm_head_t));

    period = ngx_slab_alloc(shpool, sizeof(ngx_traffic_accounting_period_t));
    if (period == NULL) {
        return NGX_ERROR;
    }

    ngx_memzero(period, sizeof(ngx_traffic_accounting_period_t));

    ngx_rbtree_init(&period->rbtree, &period->sentinel,
                    ngx_traffic_accounting_shm_insert_value);

    period->created_at_sec = ngx_time();
    period->updated_at_sec = period->created_at_sec;
    period->shpool = shpool;

    head->current = period;

    shm_zone->data = head;

    return NGX_OK;
}


ngx_int_t
ngx_traffic_accounting_shm_period_create(ngx_shm_zone_t *shm_zone,
    ngx_traffic_accounting_shm_head_t *head)
{
    ngx_slab_pool_t                 *shpool;
    ngx_traffic_accounting_period_t *period;

    shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    period = ngx_slab_alloc_locked(shpool,
                                    sizeof(ngx_traffic_accounting_period_t));
    if (period == NULL) {
        return NGX_ERROR;
    }

    ngx_memzero(period, sizeof(ngx_traffic_accounting_period_t));

    ngx_rbtree_init(&period->rbtree, &period->sentinel,
                    ngx_traffic_accounting_shm_insert_value);

    period->created_at_sec = ngx_time();
    period->updated_at_sec = period->created_at_sec;
    period->shpool = shpool;

    head->current = period;

    return NGX_OK;
}


void
ngx_traffic_accounting_shm_period_destroy(ngx_shm_zone_t *shm_zone,
    ngx_traffic_accounting_period_t *period)
{
    ngx_slab_pool_t                 *shpool;
    ngx_rbtree_t                    *rbtree;
    ngx_rbtree_node_t               *node, *sentinel;
    ngx_traffic_accounting_metrics_t *n;

    if (period == NULL) {
        return;
    }

    shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    rbtree = &period->rbtree;
    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {
        n = (ngx_traffic_accounting_metrics_t *) node;

        ngx_rbtree_delete(rbtree, node);
        ngx_slab_free_locked(shpool, n->name.data);
        ngx_slab_free_locked(shpool, n);

        node = rbtree->root;
    }

    ngx_slab_free_locked(shpool, period);
}


void
ngx_traffic_accounting_shm_worker_join(ngx_traffic_accounting_shm_head_t *head)
{
    if (head != NULL) {
        (void) ngx_atomic_fetch_add(&head->active_workers, 1);
    }
}


void
ngx_traffic_accounting_shm_worker_leave(ngx_traffic_accounting_shm_head_t *head)
{
    if (head != NULL && head->active_workers > 0) {
        (void) ngx_atomic_fetch_add(&head->active_workers,
                                     (ngx_atomic_uint_t) -1);
    }
}


ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_shm_fetch_metrics(ngx_traffic_accounting_period_t *period,
    ngx_str_t *name, ngx_log_t *log)
{
    ngx_traffic_accounting_metrics_t *n;

    n = ngx_traffic_accounting_shm_lookup_metrics(period, name);
    if (n != NULL) {
        return n;
    }

    return ngx_traffic_accounting_shm_insert_metrics(period, name, log);
}


static ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_shm_lookup_metrics(
    ngx_traffic_accounting_period_t *period, ngx_str_t *name)
{
    ngx_int_t                           rc;
    ngx_traffic_accounting_metrics_t   *n;
    ngx_rbtree_node_t                  *node, *sentinel;
    ngx_rbtree_key_t                    hash;
    ngx_rbtree_t                       *rbtree;

    hash = ngx_hash_key_lc(name->data, name->len);

    rbtree = &period->rbtree;
    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {
        if (hash != node->key) {
            node = (hash < node->key) ? node->left : node->right;
            continue;
        }

        n = (ngx_traffic_accounting_metrics_t *) node;
        rc = ngx_rstrncmp(name->data, n->name.data, name->len);

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


static ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_shm_insert_metrics(
    ngx_traffic_accounting_period_t *period, ngx_str_t *name, ngx_log_t *log)
{
    ngx_traffic_accounting_metrics_t  *metrics;
    ngx_slab_pool_t                   *shpool;
    void                              *data;

    shpool = (ngx_slab_pool_t *) period->shpool;

    metrics = ngx_slab_alloc_locked(shpool,
                                    sizeof(ngx_traffic_accounting_metrics_t));
    if (metrics == NULL) {
        return NULL;
    }

    ngx_memzero(metrics, sizeof(ngx_traffic_accounting_metrics_t));

    data = ngx_slab_alloc_locked(shpool, name->len + 1);
    if (data == NULL) {
        ngx_slab_free_locked(shpool, metrics);
        return NULL;
    }
    ngx_memcpy(data, name->data, name->len);

    metrics->name.data = data;
    metrics->name.len = name->len;

    metrics->rbnode.key = ngx_hash_key_lc(metrics->name.data, metrics->name.len);

    ngx_rbtree_insert(&period->rbtree, &metrics->rbnode);

    return metrics;
}


static void
ngx_traffic_accounting_shm_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_traffic_accounting_metrics_t  *n, *t;
    ngx_rbtree_node_t                **p;

    for ( ;; ) {
        n = (ngx_traffic_accounting_metrics_t *) node;
        t = (ngx_traffic_accounting_metrics_t *) temp;

        if (node->key != temp->key) {
            p = (node->key < temp->key) ? &temp->left : &temp->right;
        } else if (n->name.len != t->name.len) {
            p = (n->name.len < t->name.len) ? &temp->left : &temp->right;
        } else if (n->name.data == NULL) {
            p = &temp->left;
        } else if (t->name.data == NULL) {
            p = &temp->right;
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
