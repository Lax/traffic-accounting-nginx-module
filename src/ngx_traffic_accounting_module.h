
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_

#include <ngx_core.h>


#define NGX_CONF_INDEX_UNSET -128

typedef struct {
    ngx_flag_t      enable;
    ngx_log_t      *log;
    time_t          interval;
    ngx_flag_t      perturb;
} ngx_traffic_accounting_main_conf_t;

typedef struct {
    ngx_str_t       accounting_id;
    ngx_int_t       index;
} ngx_traffic_accounting_loc_conf_t;

void * ngx_traffic_accounting_create_main_conf(ngx_conf_t *cf);
char * ngx_traffic_accounting_init_main_conf(ngx_conf_t *cf, void *conf);
void * ngx_traffic_accounting_create_loc_conf(ngx_conf_t *cf);
char * ngx_traffic_accounting_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);


#endif /* _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_ */
