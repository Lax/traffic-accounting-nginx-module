
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_HTTP_ACCOUNTING_WORKER_PROCESS_H_INCLUDED_
#define _NGX_HTTP_ACCOUNTING_WORKER_PROCESS_H_INCLUDED_

#include <ngx_http.h>


extern ngx_traffic_accounting_period_t   *ngx_http_accounting_current_period;
extern ngx_traffic_accounting_period_t   *ngx_http_accounting_previous_period;

ngx_int_t ngx_http_accounting_period_create(ngx_pool_t *pool);
ngx_int_t ngx_http_accounting_period_rotate(ngx_pool_t *pool);


ngx_int_t ngx_http_accounting_worker_process_init(ngx_cycle_t *cycle);
void ngx_http_accounting_worker_process_exit(ngx_cycle_t *cycle);

ngx_int_t ngx_http_accounting_handler(ngx_http_request_t *r);

#endif /* _NGX_HTTP_ACCOUNTING_WORKER_PROCESS_H_INCLUDED_ */
