
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_

#include <ngx_core.h>


/*
 * Status Code
 */

ngx_uint_t ngx_status_bsearch(ngx_uint_t status, ngx_uint_t statuses[], ngx_uint_t size);

/*
 * Period / Metrics
 */

typedef struct {
    ngx_rbtree_node_t  rbnode;

    ngx_str_t          name;

    ngx_uint_t         nr_entries;
    ngx_uint_t         bytes_in;
    ngx_uint_t         bytes_out;
    ngx_uint_t         total_latency_ms;
    ngx_uint_t         total_upstream_latency_ms;
    ngx_uint_t        *nr_statuses;
} ngx_traffic_accounting_metrics_t;

typedef struct {
    ngx_rbtree_t       rbtree;
    ngx_rbtree_node_t  sentinel;

    ngx_pool_t        *pool;

    ngx_time_t        *created_at;
    ngx_time_t        *updated_at;
} ngx_traffic_accounting_period_t;

ngx_int_t ngx_traffic_accounting_period_init(ngx_traffic_accounting_period_t *period);
void ngx_traffic_accounting_period_insert(ngx_traffic_accounting_period_t *period, ngx_str_t *name);
void ngx_traffic_accounting_period_insert_metrics(ngx_traffic_accounting_period_t *period, ngx_traffic_accounting_metrics_t *metrics);
void ngx_traffic_accounting_period_delete(ngx_traffic_accounting_period_t *period, ngx_str_t *name);
void ngx_traffic_accounting_period_delete_metrics(ngx_traffic_accounting_period_t *period, ngx_traffic_accounting_metrics_t *metrics);
ngx_traffic_accounting_metrics_t * ngx_traffic_accounting_period_lookup_metrics(ngx_traffic_accounting_period_t *period, ngx_str_t *name);

typedef ngx_int_t (*ngx_traffic_accounting_period_iterate_func)(void *val, void *para1, void *para2);

ngx_int_t ngx_traffic_accounting_period_rbtree_iterate(ngx_traffic_accounting_period_t *period,
                                      ngx_traffic_accounting_period_iterate_func func,
                                      void *para1, void *para2 );

extern ngx_traffic_accounting_period_t   *ngxta_current_metrics;
extern ngx_traffic_accounting_period_t   *ngxta_previous_metrics;

ngx_int_t ngxta_period_init(ngx_pool_t *pool);
ngx_int_t ngxta_period_rotate(ngx_pool_t *pool);

/*
 * Log
 */

#define NGXTA_LOG_LEVEL    NGX_LOG_NOTICE


#endif /* _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_ */
