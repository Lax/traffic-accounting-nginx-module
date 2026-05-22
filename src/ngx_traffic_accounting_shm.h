
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_TRAFFIC_ACCOUNTING_SHM_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_SHM_H_INCLUDED_


#include <ngx_core.h>
#include "ngx_traffic_accounting.h"


typedef struct {
    ngx_atomic_t                       epoch;
    ngx_atomic_t                       active_workers;
    ngx_traffic_accounting_period_t   *current;
    ngx_traffic_accounting_period_t   *previous;
} ngx_traffic_accounting_shm_head_t;


char *ngx_traffic_accounting_set_zone(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf, void *tag);

ngx_int_t ngx_traffic_accounting_init_zone(ngx_shm_zone_t *shm_zone,
    void *data);

ngx_int_t ngx_traffic_accounting_shm_period_create(ngx_shm_zone_t *shm_zone,
    ngx_traffic_accounting_shm_head_t *head);

ngx_traffic_accounting_metrics_t *
ngx_traffic_accounting_shm_fetch_metrics(ngx_traffic_accounting_period_t *period,
    ngx_str_t *name, ngx_log_t *log);

void ngx_traffic_accounting_shm_period_destroy(ngx_shm_zone_t *shm_zone,
    ngx_traffic_accounting_period_t *period);

void ngx_traffic_accounting_shm_worker_join(
    ngx_traffic_accounting_shm_head_t *head);

void ngx_traffic_accounting_shm_worker_leave(
    ngx_traffic_accounting_shm_head_t *head);


#endif /* _NGX_TRAFFIC_ACCOUNTING_SHM_H_INCLUDED_ */
