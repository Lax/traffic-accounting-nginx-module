
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_HTTP_ACCOUNTING_MODULE_H_INCLUDED_
#define _NGX_HTTP_ACCOUNTING_MODULE_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_http.h>
#include "../ngx_traffic_accounting_module.h"


typedef ngx_traffic_accounting_main_conf_t    ngx_http_accounting_main_conf_t;
typedef ngx_traffic_accounting_loc_conf_t     ngx_http_accounting_loc_conf_t;

extern ngx_module_t ngx_http_accounting_module;


// Status Code. Default 0
#define NGX_HTTP_STATUS_UNSET    0
#define NGX_HTTP_MAX_STATUS      NGX_HTTP_INSUFFICIENT_STORAGE

extern ngx_uint_t ngx_http_statuses[];
extern ngx_uint_t ngx_http_statuses_len;


#endif /* _NGX_HTTP_ACCOUNTING_MODULE_H_INCLUDED_ */
