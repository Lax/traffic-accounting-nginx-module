
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_

#include <ngx_core.h>


#define NGX_CONF_INDEX_UNSET -128

typedef struct {
    ngx_str_t       accounting_id;
    ngx_int_t       index;
} ngx_traffic_accounting_loc_conf_t;

typedef struct {
    ngx_flag_t      enable;
    ngx_log_t      *log;
    time_t          interval;
    ngx_flag_t      perturb;
} ngx_traffic_accounting_main_conf_t;


#endif /* _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_ */
