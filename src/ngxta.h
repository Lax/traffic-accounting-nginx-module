#ifndef _NGX_TRAFFIC_METRICS_H_INCLUDED_
#define _NGX_TRAFFIC_METRICS_H_INCLUDED_

#include <ngx_core.h>

/*
 * Status Code
 */

// Status Code. Default 0
#define NGX_TRAFFIC_METRICS_STATUS_UNSET                   0

extern ngx_uint_t http_statuses[];

ngx_uint_t statuses_count(ngx_uint_t statuses[]);
ngx_uint_t statuses_bsearch(ngx_uint_t statuses[], ngx_uint_t *status);

/*
 * Period / Metrics
 */

typedef struct {
    ngx_rbtree_node_t  rbnode;

    ngx_str_t          name;

    ngx_uint_t         nr_entities;
    ngx_uint_t         bytes_in;
    ngx_uint_t         bytes_out;
    ngx_uint_t         total_latency_ms;
    ngx_uint_t         total_upstream_latency_ms;
    ngx_uint_t        *nr_statuses;
} ngxta_metrics_rbnode_t;

typedef struct {
    ngx_rbtree_t       rbtree;
    ngx_rbtree_node_t  sentinel;

    ngx_pool_t        *pool;

    ngx_time_t        *created_at;
    ngx_time_t        *updated_at;
} ngxta_period_rbtree_t;

ngx_int_t ngxta_period_rbtree_init(ngxta_period_rbtree_t *period);
void ngxta_period_rbtree_insert(ngxta_period_rbtree_t *period, ngx_str_t *name);
void ngxta_period_rbtree_insert_metrics(ngxta_period_rbtree_t *period, ngxta_metrics_rbnode_t *metrics);
void ngxta_period_rbtree_delete(ngxta_period_rbtree_t *period, ngx_str_t *name);
void ngxta_period_rbtree_delete_metrics(ngxta_period_rbtree_t *period, ngxta_metrics_rbnode_t *metrics);
ngxta_metrics_rbnode_t * ngxta_period_rbtree_lookup_metrics(ngxta_period_rbtree_t *period, ngx_str_t *name);

typedef ngx_int_t (*ngxta_period_rbtree_iterate_func)(void *val, void *para1, void *para2);

ngx_int_t ngxta_period_rbtree_iterate(ngxta_period_rbtree_t *period,
                ngxta_period_rbtree_iterate_func func, void *para1, void *para2);

extern ngxta_period_rbtree_t  *ngxta_current_metrics;
extern ngxta_period_rbtree_t  *ngxta_previous_metrics;

ngx_int_t ngxta_period_init(ngx_pool_t *pool);
ngx_int_t ngxta_period_rotate(ngx_pool_t *pool);

#endif /* _NGX_TRAFFIC_METRICS_H_INCLUDED_ */
