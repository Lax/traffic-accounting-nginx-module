
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_

#include <ngx_core.h>
#include "ngx_traffic_accounting_compat.h"
#include "ngx_traffic_accounting.h"


#define NGX_CONF_INDEX_UNSET -128

/* Limits enforced by ngx_traffic_accounting_init_main_conf() */
#define NGXTA_MAX_METRICS_MIN      16
#define NGXTA_MAX_METRICS_DEFAULT  16384
#define NGXTA_MAX_METRICS_MAX      1048576
#define NGXTA_MAX_INTERVAL         (30 * 86400)

typedef struct {
    ngx_flag_t      enable;
    ngx_log_t      *log;
    time_t          interval;
    ngx_flag_t      perturb;
    ngx_uint_t      max_metrics;

    ngx_traffic_accounting_period_t   *current;
} ngx_traffic_accounting_main_conf_t;

typedef struct {
    ngx_str_t       accounting_id;
    ngx_int_t       index;
} ngx_traffic_accounting_loc_conf_t;

void * ngx_traffic_accounting_create_main_conf(ngx_conf_t *cf);
char * ngx_traffic_accounting_init_main_conf(ngx_conf_t *cf, void *conf);
void * ngx_traffic_accounting_create_loc_conf(ngx_conf_t *cf);
char * ngx_traffic_accounting_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

char * ngx_traffic_accounting_set_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

typedef ngx_int_t (*ngx_get_variable_index_pt) (ngx_conf_t *cf, ngx_str_t *name);
char * ngx_traffic_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf,
    ngx_get_variable_index_pt get_variable_index);


typedef ngx_traffic_accounting_loc_conf_t *(*ngx_get_loc_conf_pt) (void *entry);
typedef ngx_variable_value_t *(*ngx_get_indexed_variable_pt) (void *entry, ngx_uint_t index);
ngx_str_t * ngx_traffic_accounting_get_accounting_id(ngx_str_t *id, void *entry,
    ngx_get_loc_conf_pt get_loc_conf, ngx_get_indexed_variable_pt get_indexed_variable);


ngx_traffic_accounting_period_t * ngx_traffic_accounting_period_create(ngx_traffic_accounting_main_conf_t *amcf);


#endif /* _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_ */
