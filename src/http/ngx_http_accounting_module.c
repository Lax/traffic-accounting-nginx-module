
/*
 * Copyright (C) Liu Lantao
 */


#include <ngx_http.h>
#include <syslog.h>
#include "ngx_http_accounting_module.h"
#include "../ngx_traffic_accounting_shm.h"


static char entry_n[] = "requests";
static u_char *ngx_http_accounting_title = (u_char *)"NgxAccounting";

static ngx_int_t ngx_http_accounting_init(ngx_conf_t *cf);

static ngx_int_t ngx_http_accounting_process_init(ngx_cycle_t *cycle);
static void ngx_http_accounting_process_exit(ngx_cycle_t *cycle);

static void worker_process_alarm_handler(ngx_event_t *ev);

static char *ngx_http_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_accounting_set_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_http_accounting_request_handler(ngx_http_request_t *r);
static ngx_http_accounting_loc_conf_t *ngx_http_accounting_get_loc_conf(void *entry);
static ngx_str_t *ngx_http_accounting_get_accounting_id(ngx_http_request_t *r);

static ngx_traffic_accounting_metrics_t *
ngx_http_per_process_fetch_metrics(void *context, ngx_str_t *name);

static ngx_traffic_accounting_metrics_t *
ngx_http_shm_fetch_metrics(void *context, ngx_str_t *name);


static ngx_command_t  ngx_http_accounting_commands[] = {
    { ngx_string("accounting"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_accounting_main_conf_t, enable),
      NULL},

    { ngx_string("accounting_interval"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_sec_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_accounting_main_conf_t, interval),
      NULL},

    { ngx_string("accounting_perturb"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_accounting_main_conf_t, perturb),
      NULL},

    { ngx_string("accounting_period_reset"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_traffic_accounting_set_period_reset,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL},

    { ngx_string("accounting_log"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_1MORE,
      ngx_traffic_accounting_set_log,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL},

    { ngx_string("accounting_id"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_accounting_set_accounting_id,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL},

    { ngx_string("accounting_skip"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_traffic_accounting_loc_conf_t, skip),
      NULL},

    { ngx_string("accounting_zone"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE2,
      ngx_http_accounting_set_zone,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL},

    ngx_null_command
};


static ngx_http_module_t  ngx_http_accounting_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_http_accounting_init,               /* postconfiguration */
    ngx_traffic_accounting_create_main_conf,/* create main configuration */
    ngx_traffic_accounting_init_main_conf,  /* init main configuration */
    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */
    ngx_traffic_accounting_create_loc_conf, /* create location configuration */
    ngx_traffic_accounting_merge_loc_conf   /* merge location configuration */
};


ngx_module_t ngx_http_accounting_module = {
    NGX_MODULE_V1,
    &ngx_http_accounting_ctx,               /* module context */
    ngx_http_accounting_commands,           /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    ngx_http_accounting_process_init,       /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    ngx_http_accounting_process_exit,       /* exit process */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_accounting_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt               *h;
    ngx_http_core_main_conf_t         *cmcf;
    ngx_http_accounting_main_conf_t   *amcf;

    amcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_accounting_module);
    if (!amcf->enable) {
        return NGX_OK;
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_accounting_request_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_http_accounting_process_init(ngx_cycle_t *cycle)
{
    ngx_http_accounting_main_conf_t   *amcf;
    ngx_event_t                       *ev;
    time_t                             perturb_factor = 1000;

    amcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_accounting_module);
    if (!amcf->enable) {
        return NGX_OK;
    }

    if (amcf->log != NULL) {
        ngx_log_error(NGXTA_LOG_LEVEL, amcf->log, 0,
                      "pid:%i|start http traffic accounting", ngx_getpid());
    } else {
        openlog((char *)ngx_http_accounting_title, LOG_NDELAY, LOG_SYSLOG);
        syslog(LOG_INFO, "pid:%i|start http traffic accounting", ngx_getpid());
    }

    if (amcf->shm_zone != NULL) {
        amcf->shm_head = amcf->shm_zone->data;
        amcf->fetch_metrics = ngx_http_shm_fetch_metrics;

        ngx_traffic_accounting_shm_worker_join(amcf->shm_head);
    } else {
        amcf->fetch_metrics = ngx_http_per_process_fetch_metrics;

        if (amcf->current == NULL) {
            if (ngx_traffic_accounting_period_create(amcf) != NGX_OK)
                return NGX_ERROR;
        }
    }

    ev = ngx_pcalloc(cycle->pool, sizeof(ngx_event_t));
    if (ev == NULL)
        return NGX_ERROR;

    ev->data = NULL;
    ev->log = cycle->log;
    ev->handler = worker_process_alarm_handler;
    ev->cancelable = 1;

    if (amcf->perturb) {
        srand(ngx_getpid() * ngx_max_module + ngx_http_accounting_module.ctx_index);
        perturb_factor = (1000 - rand() % 200);
    }

    ngx_add_timer(ev, amcf->interval * perturb_factor);

    return NGX_OK;
}


static void
ngx_http_accounting_process_exit(ngx_cycle_t *cycle)
{
    ngx_http_accounting_main_conf_t   *amcf;

    amcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_accounting_module);
    if (!amcf->enable) {
        return;
    }

    worker_process_alarm_handler(NULL);

    if (amcf->shm_zone != NULL) {
        ngx_traffic_accounting_shm_worker_leave(amcf->shm_head);
    }

    if (amcf->log != NULL) {
        ngx_log_error(NGXTA_LOG_LEVEL, amcf->log, 0,
                      "pid:%i|stop http traffic accounting", ngx_getpid());
    } else {
        syslog(LOG_INFO, "pid:%i|stop http traffic accounting", ngx_getpid());
    }
}


static ngx_int_t
worker_process_export_metrics(void *val, void *para1, void *para2)
{
    ngx_http_accounting_main_conf_t   *amcf;
    ngx_traffic_accounting_period_t   *period;
    ngx_uint_t                         nr_workers;
    ngx_int_t                          rc;

    amcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_accounting_module);
    period = (ngx_traffic_accounting_period_t *) para1;
    nr_workers = (ngx_uint_t) (uintptr_t) para2;

    rc = ngx_traffic_accounting_log_metrics(val, period, nr_workers,
                                            amcf->log, entry_n,
                                            ngx_http_statuses,
                                            ngx_http_statuses_len );
    if (rc == NGX_OK) {
        /* NGX_DONE -> auto-destroy node for per-process path */
        return (nr_workers > 0) ? NGX_OK : NGX_DONE;
    }

    return rc;
}


static void
worker_process_alarm_handler(ngx_event_t *ev)
{
    ngx_http_accounting_main_conf_t    *amcf;
    ngx_traffic_accounting_shm_head_t  *shm_head;
    ngx_traffic_accounting_period_t    *period;
    ngx_slab_pool_t                    *shpool;
    ngx_uint_t                          nr_workers;
    ngx_atomic_uint_t                   old_epoch;

    amcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_accounting_module);

    if (amcf->shm_zone != NULL) {
        shm_head = amcf->shm_head;
        shpool = (ngx_slab_pool_t *) amcf->shm_zone->shm.addr;

        old_epoch = shm_head->epoch;

        if (!ngx_atomic_cmp_set(&shm_head->epoch, old_epoch, old_epoch + 1))
        {
            if (!ngx_exiting && ev != NULL) {
                ngx_add_timer(ev, (ngx_msec_t)amcf->interval * 1000);
            }
            return;
        }

        ngx_shmtx_lock(&shpool->mutex);

        if (shm_head->previous != NULL) {
            ngx_traffic_accounting_shm_period_destroy(amcf->shm_zone,
                                                       shm_head->previous);
        }
        shm_head->previous = shm_head->current;
        shm_head->current = NULL;

        if (ngx_traffic_accounting_shm_period_create(amcf->shm_zone, shm_head)
            != NGX_OK)
        {
            ngx_shmtx_unlock(&shpool->mutex);
            if (!ngx_exiting && ev != NULL) {
                ngx_add_timer(ev, (ngx_msec_t)amcf->interval * 1000);
            }
            return;
        }

        if (ngx_traffic_accounting_check_reset(amcf)) {
            ngx_traffic_accounting_shm_period_destroy(amcf->shm_zone,
                                                       shm_head->current);
            if (ngx_traffic_accounting_shm_period_create(amcf->shm_zone,
                                                          shm_head)
                != NGX_OK)
            {
                ngx_shmtx_unlock(&shpool->mutex);
                if (!ngx_exiting && ev != NULL) {
                    ngx_add_timer(ev, (ngx_msec_t)amcf->interval * 1000);
                }
                return;
            }
        }

        ngx_shmtx_unlock(&shpool->mutex);

        period = shm_head->previous;
        nr_workers = (ngx_uint_t) shm_head->active_workers;

        ngx_traffic_accounting_period_rbtree_iterate(period,
                                  worker_process_export_metrics,
                                  (void *) period,
                                  (void *) (uintptr_t) nr_workers );

        ngx_shmtx_lock(&shpool->mutex);
        ngx_traffic_accounting_shm_period_destroy(amcf->shm_zone, period);
        shm_head->previous = NULL;
        ngx_shmtx_unlock(&shpool->mutex);

    } else {
        /* per-process path */
        ngx_traffic_accounting_period_rotate(amcf);

        period = amcf->previous;

        ngx_traffic_accounting_period_rbtree_iterate(period,
                                  worker_process_export_metrics,
                                  (void *) period,
                                  NULL );

        if (ngx_traffic_accounting_check_reset(amcf)) {
            ngx_traffic_accounting_period_clear(amcf->current);
        }
    }

    if (ngx_exiting || ev == NULL)
        return;

    ngx_add_timer(ev, (ngx_msec_t)amcf->interval * 1000);
}


static char *
ngx_http_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return ngx_traffic_accounting_set_accounting_id(cf, cmd, conf, ngx_http_get_variable_index);
}


static char *
ngx_http_accounting_set_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return ngx_traffic_accounting_set_zone(cf, cmd, conf,
                                            &ngx_http_accounting_module);
}


static ngx_int_t
ngx_http_accounting_request_handler(ngx_http_request_t *r)
{
    ngx_str_t                          *accounting_id;
    ngx_traffic_accounting_metrics_t   *metrics;
    ngx_http_accounting_main_conf_t    *amcf;
    ngx_slab_pool_t                    *shpool = NULL;

    ngx_uint_t                   status, i;
    ngx_time_t                  *tp = ngx_timeofday();
    ngx_msec_int_t               ms = 0;
    ngx_http_upstream_state_t   *state;

    if (ngx_http_accounting_get_loc_conf(r)->skip) {
        return NGX_DECLINED;
    }

    accounting_id = ngx_http_accounting_get_accounting_id(r);
    if (accounting_id == NULL) { return NGX_ERROR; }

    amcf = ngx_http_get_module_main_conf(r, ngx_http_accounting_module);

    if (amcf->shm_zone != NULL) {
        shpool = (ngx_slab_pool_t *) amcf->shm_zone->shm.addr;
        ngx_shmtx_lock(&shpool->mutex);
    }

    metrics = amcf->fetch_metrics(amcf, accounting_id);
    if (metrics == NULL) {
        if (amcf->shm_zone != NULL) {
            ngx_shmtx_unlock(&shpool->mutex);
        }
        return NGX_ERROR;
    }

    metrics->nr_entries += 1;
    metrics->bytes_in += r->request_length;
    metrics->bytes_out += r->connection->sent;

    if (r->err_status) {
        status = r->err_status;
    } else if (r->headers_out.status) {
        status = r->headers_out.status;
    } else {
        status = NGX_HTTP_STATUS_UNSET;
    }

    metrics->nr_status[ngx_status_bsearch(status, ngx_http_statuses, ngx_http_statuses_len)] += 1;

    ms = (ngx_msec_int_t)((tp->sec - r->start_sec) * 1000 + (tp->msec - r->start_msec));
    ms = ngx_max(ms, 0);

    metrics->total_latency_ms += ms;

    if (r->upstream_states != NULL && r->upstream_states->nelts != 0) {
        ms = 0;
        state = r->upstream_states->elts;

        for (i = 0; i < r->upstream_states->nelts; i++) {
            if (state[i].status) {
#if (nginx_version < 1009000)
                ms += (state[i].response_sec * 1000 + state[i].response_msec);
#else
                ms += state[i].response_time;
#endif
            }
        }

        metrics->total_upstream_latency_ms += ms;
    }

    if (amcf->shm_zone != NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
    }

    return NGX_DECLINED;
}


static ngx_traffic_accounting_metrics_t *
ngx_http_per_process_fetch_metrics(void *context, ngx_str_t *name)
{
    ngx_traffic_accounting_main_conf_t *amcf = context;
    ngx_traffic_accounting_metrics_t   *metrics;

    metrics = ngx_traffic_accounting_period_fetch_metrics(amcf->current,
                                                           name, amcf->log);
    if (metrics != NULL) {
        amcf->current->updated_at_sec = ngx_time();
    }
    return metrics;
}


static ngx_traffic_accounting_metrics_t *
ngx_http_shm_fetch_metrics(void *context, ngx_str_t *name)
{
    ngx_traffic_accounting_main_conf_t *amcf = context;
    ngx_traffic_accounting_metrics_t   *metrics;

    metrics = ngx_traffic_accounting_shm_fetch_metrics(amcf->shm_head->current,
                                                        name, amcf->log);
    if (metrics != NULL) {
        amcf->shm_head->current->updated_at_sec = ngx_time();
    }
    return metrics;
}


static ngx_http_accounting_loc_conf_t *
ngx_http_accounting_get_loc_conf(void *entry)
{
    return ngx_http_get_module_loc_conf((ngx_http_request_t *)entry, ngx_http_accounting_module);
}

static ngx_http_variable_value_t *
ngx_http_accounting_get_indexed_variable(void *entry, ngx_uint_t index)
{
    return ngx_http_get_indexed_variable((ngx_http_request_t *)entry, index);
}

static ngx_str_t *
ngx_http_accounting_get_accounting_id(ngx_http_request_t *r)
{
    return ngx_traffic_accounting_get_accounting_id(r, ngx_http_accounting_get_loc_conf, ngx_http_accounting_get_indexed_variable);
}
