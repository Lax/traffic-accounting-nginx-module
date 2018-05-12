
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_STREAM_ACCOUNTING_MODULE_H_INCLUDED_
#define _NGX_STREAM_ACCOUNTING_MODULE_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_stream.h>
#include "../ngx_traffic_accounting_module.h"


typedef ngx_traffic_accounting_main_conf_t    ngx_stream_accounting_main_conf_t;
typedef ngx_traffic_accounting_loc_conf_t     ngx_stream_accounting_loc_conf_t;

extern ngx_module_t ngx_stream_accounting_module;


// Status Code. Default 0
#define NGX_STREAM_STATUS_UNSET    0
#define NGX_STREAM_MAX_STATUS      NGX_STREAM_SERVICE_UNAVAILABLE

extern ngx_uint_t ngx_stream_statuses[];
extern ngx_uint_t ngx_stream_statuses_len;


#endif /* _NGX_STREAM_ACCOUNTING_MODULE_H_INCLUDED_ */
