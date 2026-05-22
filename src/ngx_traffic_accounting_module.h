
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_

#include <ngx_core.h>
#include "ngx_traffic_accounting.h"
#include "ngx_traffic_accounting_shm.h"


#define NGX_CONF_INDEX_UNSET -128

typedef enum {
    NGX_TA_PERIOD_RESET_NONE    = 0,
    NGX_TA_PERIOD_RESET_HOURLY  = 1,
    NGX_TA_PERIOD_RESET_DAILY   = 2,
    NGX_TA_PERIOD_RESET_WEEKLY  = 3,
    NGX_TA_PERIOD_RESET_MONTHLY = 4
} ngx_ta_period_reset_t;

typedef ngx_traffic_accounting_metrics_t *
    (*ngx_ta_fetch_metrics_pt)(void *context, ngx_str_t *name);

typedef struct {
    ngx_flag_t      enable;
    ngx_log_t      *log;
    time_t          interval;
    ngx_flag_t      perturb;
    ngx_ta_period_reset_t   period_reset;
    ngx_uint_t              last_reset_day;

    ngx_shm_zone_t                     *shm_zone;
    ngx_traffic_accounting_shm_head_t   *shm_head;

    ngx_ta_fetch_metrics_pt             fetch_metrics;

    ngx_traffic_accounting_period_t   *current;
    ngx_traffic_accounting_period_t   *previous;
} ngx_traffic_accounting_main_conf_t;

typedef struct {
    ngx_str_t       accounting_id;
    ngx_int_t       index;
    ngx_flag_t      skip;
    void           *cv;
} ngx_traffic_accounting_loc_conf_t;

void * ngx_traffic_accounting_create_main_conf(ngx_conf_t *cf);
char * ngx_traffic_accounting_init_main_conf(ngx_conf_t *cf, void *conf);
void * ngx_traffic_accounting_create_loc_conf(ngx_conf_t *cf);
char * ngx_traffic_accounting_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

char * ngx_traffic_accounting_set_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char * ngx_traffic_accounting_set_period_reset(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

typedef ngx_int_t (*ngx_get_variable_index_pt) (ngx_conf_t *cf, ngx_str_t *name);
typedef ngx_int_t (*ngx_compile_cv_pt) (ngx_conf_t *cf, ngx_str_t *value, void **cv);
char * ngx_traffic_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf,
    ngx_get_variable_index_pt get_variable_index, ngx_compile_cv_pt compile_cv);


typedef ngx_traffic_accounting_loc_conf_t *(*ngx_get_loc_conf_pt) (void *entry);
typedef ngx_variable_value_t *(*ngx_get_indexed_variable_pt) (void *entry, ngx_uint_t index);
typedef ngx_int_t (*ngx_run_cv_pt) (void *entry, void *cv, ngx_str_t *out);
ngx_str_t * ngx_traffic_accounting_get_accounting_id(void *entry, ngx_get_loc_conf_pt get_loc_conf,
    ngx_get_indexed_variable_pt get_indexed_variable, ngx_run_cv_pt run_cv);


ngx_int_t ngx_traffic_accounting_period_create(ngx_traffic_accounting_main_conf_t *amcf);
ngx_int_t ngx_traffic_accounting_period_rotate(ngx_traffic_accounting_main_conf_t *amcf);
ngx_int_t ngx_traffic_accounting_check_reset(ngx_traffic_accounting_main_conf_t *amcf);
void ngx_traffic_accounting_period_clear(ngx_traffic_accounting_period_t *period);


#endif /* _NGX_TRAFFIC_ACCOUNTING_MODULE_H_INCLUDED_ */
