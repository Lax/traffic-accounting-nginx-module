/*
 * Test: ngx_traffic_accounting_period_insert NULL-pointer safety
 *
 * Compile: cc -I test/ -I src/ -Wall -Werror \
 *              test/test_null_alloc.c -o test_null_alloc
 */

#include "../src/ngx_traffic_accounting_period_metrics.c"

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>


/* ── Mock state ── */

static int   fail_after  = 0;   /* 0 = no failure, N = fail Nth ngx_calloc */
static int   alloc_count = 0;   /* call counter */


/* ── Mock ngx_calloc ── */

void *
ngx_calloc(size_t size, ngx_log_t *log)
{
    alloc_count++;

    if (fail_after > 0 && alloc_count == fail_after) {
        fprintf(stderr, "  [mock] ngx_calloc #%d → NULL (fail_after=%d)\n",
                alloc_count, fail_after);
        return NULL;
    }

    void *p = calloc(1, size);
    fprintf(stderr, "  [mock] ngx_calloc #%d → %p (size=%zu)\n",
            alloc_count, p, size);
    return p;
}


/* ── Mock ngx_free ── */

void
ngx_free(void *p)
{
    fprintf(stderr, "  [mock] ngx_free(%p)\n", p);
    free(p);
}


/* ── Rbtree implementation (minimal, sufficient for test) ── */

void
ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    node->left   = tree->sentinel;
    node->right  = tree->sentinel;
    node->parent = NULL;
    ngx_rbt_red(node);

    if (tree->root == tree->sentinel) {
        tree->root = node;
        return;
    }

    tree->insert(tree->root, node, tree->sentinel);
}


void
ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *subst, *child;

    if (node->left == tree->sentinel || node->right == tree->sentinel) {
        subst = node;
    } else {
        subst = node->right;
        while (subst->left != tree->sentinel) {
            subst = subst->left;
        }
    }

    child = (subst->left != tree->sentinel) ? subst->left : subst->right;

    if (subst->parent == NULL) {
        tree->root = child;
    } else if (subst == subst->parent->left) {
        subst->parent->left = child;
    } else {
        subst->parent->right = child;
    }

    if (child != tree->sentinel) {
        child->parent = subst->parent;
    }

    if (subst != node) {
        subst->left   = node->left;
        subst->right  = node->right;
        subst->parent = node->parent;

        if (node->left != tree->sentinel) {
            node->left->parent = subst;
        }
        if (node->right != tree->sentinel) {
            node->right->parent = subst;
        }

        if (node->parent == NULL) {
            tree->root = subst;
        } else if (node == node->parent->left) {
            node->parent->left = subst;
        } else {
            node->parent->right = subst;
        }
    }
}


/* ── Mock hash ── */

ngx_rbtree_key_t
ngx_hash_key_lc(u_char *data, size_t len)
{
    ngx_rbtree_key_t  h = 0;
    size_t            i;

    for (i = 0; i < len; i++) {
        h = h * 211 + data[i];
    }

    return h;
}


/* ── Crash catcher ── */

static sigjmp_buf  crash_env;
static int         crash_signo = 0;

static void
crash_handler(int sig)
{
    crash_signo = sig;
    siglongjmp(crash_env, 1);
}


/* ── Test helpers ── */

static int  n_pass = 0;
static int  n_fail = 0;

static void
reset_mock(void)
{
    alloc_count = 0;
    fail_after  = 0;
    crash_signo = 0;
}


/* ── Test cases ── */

static int
test_metrics_alloc_fails(void)
{
    printf("[Test 1] ngx_calloc for metrics  → NULL ... ");
    fflush(stdout);

    ngx_traffic_accounting_period_t   period;
    ngx_log_t                         log;
    u_char                            name_data[] = "test_id";
    ngx_str_t                         name = { 7, name_data };

    reset_mock();
    fail_after = 1;   /* 1st ngx_calloc (metrics) returns NULL */

    ngx_traffic_accounting_period_init(&period);

    crash_signo = 0;
    if (sigsetjmp(crash_env, 1) == 0) {
        ngx_traffic_accounting_period_insert(&period, &name, &log);
        printf("PASS  (no crash)\n");
        n_pass++;
        return 1;
    } else {
        printf("FAIL  (SIGSEGV/%d)\n", crash_signo);
        n_fail++;
        return 0;
    }
}


static int
test_data_alloc_fails(void)
{
    printf("[Test 2] ngx_calloc for data    → NULL ... ");
    fflush(stdout);

    ngx_traffic_accounting_period_t   period;
    ngx_log_t                         log;
    u_char                            name_data[] = "test_id";
    ngx_str_t                         name = { 7, name_data };

    reset_mock();
    fail_after = 2;   /* 2nd ngx_calloc (name data) returns NULL */

    ngx_traffic_accounting_period_init(&period);

    crash_signo = 0;
    if (sigsetjmp(crash_env, 1) == 0) {
        ngx_traffic_accounting_period_insert(&period, &name, &log);
        printf("PASS  (no crash)\n");
        n_pass++;
        return 1;
    } else {
        printf("FAIL  (SIGSEGV/%d)\n", crash_signo);
        n_fail++;
        return 0;
    }
}


static int
test_normal_alloc(void)
{
    printf("[Test 3] ngx_calloc succeeds    → ... ");
    fflush(stdout);

    ngx_traffic_accounting_period_t   period;
    ngx_log_t                         log;
    u_char                            name_data[] = "test_id";
    ngx_str_t                         name = { 7, name_data };

    reset_mock();
    fail_after = 0;   /* no failure */

    ngx_traffic_accounting_period_init(&period);

    crash_signo = 0;
    if (sigsetjmp(crash_env, 1) == 0) {
        ngx_traffic_accounting_period_insert(&period, &name, &log);
        printf("PASS  (no crash)\n");
        n_pass++;
        return 1;
    } else {
        printf("FAIL  (unexpected SIGSEGV/%d)\n", crash_signo);
        n_fail++;
        return 0;
    }
}


static int
test_period_clear_safety(void)
{
    printf("[Test 5] period_clear safety test → ... ");
    fflush(stdout);

    ngx_traffic_accounting_period_t   period;
    ngx_log_t                         log;
    u_char                            name_data[] = "test_clear";
    ngx_str_t                         name = { 10, name_data };
    int                               i;

    reset_mock();
    fail_after = 0;

    ngx_traffic_accounting_period_init(&period);

    for (i = 0; i < 3; i++) {
        name.data[0] = 'a' + i;
        ngx_traffic_accounting_period_insert(&period, &name, &log);
    }

    crash_signo = 0;
    if (sigsetjmp(crash_env, 1) == 0) {
        ngx_traffic_accounting_period_clear(&period);

        if (period.rbtree.root == period.rbtree.sentinel) {
            printf("PASS  (no crash, tree empty)\n");
            n_pass++;
            return 1;
        } else {
            printf("FAIL  (tree not empty after clear)\n");
            n_fail++;
            return 0;
        }
    } else {
        printf("FAIL  (SIGSEGV/%d)\n", crash_signo);
        n_fail++;
        return 0;
    }
}


static int
test_null_name_compare(void)
{
    printf("[Test 4] insert_value with NULL name.data → ... ");
    fflush(stdout);

    ngx_traffic_accounting_period_t   period;
    ngx_log_t                         log;
    u_char                            name_data[] = "AAAA";
    ngx_str_t                         name = { 4, name_data };
    ngx_traffic_accounting_metrics_t  *existing, *bad;

    reset_mock();
    fail_after = 0;

    ngx_traffic_accounting_period_init(&period);

    ngx_traffic_accounting_period_insert(&period, &name, &log);

    existing = ngx_traffic_accounting_period_lookup_metrics(&period, &name);

    bad = ngx_calloc(sizeof(ngx_traffic_accounting_metrics_t), &log);
    bad->name.len = 4;
    bad->rbnode.key = existing->rbnode.key;

    crash_signo = 0;
    if (sigsetjmp(crash_env, 1) == 0) {
        ngx_rbtree_insert(&period.rbtree, &bad->rbnode);
        printf("PASS  (no crash)\n");
        n_pass++;
        return 1;
    } else {
        printf("FAIL  (SIGSEGV/%d)\n", crash_signo);
        n_fail++;
        return 0;
    }
}


/* ── Main ── */

int
main(void)
{
    signal(SIGSEGV, crash_handler);

    test_metrics_alloc_fails();
    test_data_alloc_fails();
    test_normal_alloc();
    test_null_name_compare();
    test_period_clear_safety();

    printf("\n%d / %d passed\n", n_pass, n_pass + n_fail);

    return n_fail > 0 ? 1 : 0;
}
