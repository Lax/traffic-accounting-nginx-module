
/*
 * Copyright (C) Liu Lantao
 */


#include <ngx_stream.h>
#include <syslog.h>
#include "ngx_stream_accounting_module.h"


static char entry_n[] = "sessions";
static u_char *ngx_stream_accounting_title = (u_char *)"NgxAccounting";

static ngx_int_t ngx_stream_accounting_init(ngx_conf_t *cf);

static ngx_int_t ngx_stream_accounting_process_init(ngx_cycle_t *cycle);
static void ngx_stream_accounting_process_exit(ngx_cycle_t *cycle);

static void worker_process_alarm_handler(ngx_event_t *ev);

static char *ngx_stream_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_stream_accounting_session_handler(ngx_stream_session_t *r);
static ngx_str_t *ngx_stream_accounting_get_accounting_id(ngx_stream_session_t *r);


static ngx_command_t  ngx_stream_accounting_commands[] = {
    { ngx_string("accounting"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_stream_accounting_main_conf_t, enable),
      NULL},

    { ngx_string("accounting_interval"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_sec_slot,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_stream_accounting_main_conf_t, interval),
      NULL},

    { ngx_string("accounting_perturb"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_stream_accounting_main_conf_t, perturb),
      NULL},

    { ngx_string("accounting_log"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_1MORE,
      ngx_traffic_accounting_set_log,
      NGX_STREAM_MAIN_CONF_OFFSET,
      0,
      NULL},

    { ngx_string("accounting_id"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_stream_accounting_set_accounting_id,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL},

    ngx_null_command
};


static ngx_stream_module_t  ngx_stream_accounting_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_stream_accounting_init,             /* postconfiguration */
    ngx_traffic_accounting_create_main_conf,/* create main configuration */
    ngx_traffic_accounting_init_main_conf,  /* init main configuration */
    ngx_traffic_accounting_create_loc_conf, /* create server configuration */
    ngx_traffic_accounting_merge_loc_conf   /* merge server configuration */
};


ngx_module_t ngx_stream_accounting_module = {
    NGX_MODULE_V1,
    &ngx_stream_accounting_ctx,             /* module context */
    ngx_stream_accounting_commands,         /* module directives */
    NGX_STREAM_MODULE,                      /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    ngx_stream_accounting_process_init,     /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    ngx_stream_accounting_process_exit,     /* exit process */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_stream_accounting_init(ngx_conf_t *cf)
{
    ngx_stream_handler_pt               *h;
    ngx_stream_core_main_conf_t         *cmcf;
    ngx_stream_accounting_main_conf_t   *amcf;

    amcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_accounting_module);
    if (!amcf->enable) {
        return NGX_OK;
    }

    cmcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_STREAM_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_stream_accounting_session_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_stream_accounting_process_init(ngx_cycle_t *cycle)
{
    ngx_stream_accounting_main_conf_t   *amcf;
    ngx_event_t                         *ev;
    time_t                               perturb_factor = 1000;

    amcf = ngx_stream_cycle_get_module_main_conf(cycle, ngx_stream_accounting_module);
    if (!amcf->enable) {
        return NGX_OK;
    }

    if (amcf->log != NULL) {
        ngx_log_error(NGXTA_LOG_LEVEL, amcf->log, 0, "pid:%i|start stream traffic accounting", ngx_getpid());
    } else {
        openlog((char *)ngx_stream_accounting_title, LOG_NDELAY, LOG_SYSLOG);
        syslog(LOG_INFO, "pid:%i|start stream traffic accounting", ngx_getpid());
    }

    if (amcf->current == NULL) {
        if (ngx_traffic_accounting_period_create(amcf) != NGX_OK)
            return NGX_ERROR;
    }

    ev = ngx_pcalloc(cycle->pool, sizeof(ngx_event_t));
    if (ev == NULL)
        return NGX_ERROR;

    ev->data = NULL;
    ev->log = cycle->log;
    ev->handler = worker_process_alarm_handler;
    ev->cancelable = 1;

    if (amcf->perturb) {
        srand(ngx_getpid() * ngx_max_module + ngx_stream_accounting_module.ctx_index);
        perturb_factor = (1000 - rand() % 200);
    }

    ngx_add_timer(ev, amcf->interval * perturb_factor);

    return NGX_OK;
}


static void
ngx_stream_accounting_process_exit(ngx_cycle_t *cycle)
{
    ngx_stream_accounting_main_conf_t   *amcf;

    amcf = ngx_stream_cycle_get_module_main_conf(cycle, ngx_stream_accounting_module);
    if (!amcf->enable) {
        return;
    }

    worker_process_alarm_handler(NULL);

    if (amcf->log != NULL) {
        ngx_log_error(NGXTA_LOG_LEVEL, amcf->log, 0, "pid:%i|stop stream traffic accounting", ngx_getpid());
    } else {
        syslog(LOG_INFO, "pid:%i|stop stream traffic accounting", ngx_getpid());
    }
}

static ngx_int_t
worker_process_export_metrics(void *val, void *para1, void *para2)
{
    ngx_stream_accounting_main_conf_t   *amcf;
    ngx_int_t                            rc;

    amcf = ngx_stream_cycle_get_module_main_conf(ngx_cycle, ngx_stream_accounting_module);

    rc = ngx_traffic_accounting_log_metrics(val, para1, para2,
                                            amcf->log, entry_n,
                                            ngx_stream_statuses,
                                            ngx_stream_statuses_len );
    if (rc == NGX_OK) { return NGX_DONE; }  /* NGX_DONE -> destroy node */

    return rc;
}

static void
worker_process_alarm_handler(ngx_event_t *ev)
{
    ngx_stream_accounting_main_conf_t   *amcf;

    amcf = ngx_stream_cycle_get_module_main_conf(ngx_cycle, ngx_stream_accounting_module);

    ngx_traffic_accounting_period_rotate(amcf);
    ngx_traffic_accounting_period_rbtree_iterate(amcf->previous,
                              worker_process_export_metrics,
                              amcf->previous->created_at,
                              amcf->previous->updated_at );

    if (ngx_exiting || ev == NULL)
        return;

    ngx_add_timer(ev, (ngx_msec_t)amcf->interval * 1000);
}


static char *
ngx_stream_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return ngx_traffic_accounting_set_accounting_id(cf, cmd, conf, ngx_stream_get_variable_index);
}


static ngx_int_t
ngx_stream_accounting_session_handler(ngx_stream_session_t *s)
{
    ngx_str_t                           *accounting_id;
    ngx_traffic_accounting_metrics_t    *metrics;
    ngx_stream_accounting_main_conf_t   *amcf;

    ngx_uint_t                     status, i;
    ngx_time_t                    *tp = ngx_timeofday();
    ngx_msec_int_t                 ms = 0;
    ngx_stream_upstream_state_t   *state;

    accounting_id = ngx_stream_accounting_get_accounting_id(s);
    if (accounting_id == NULL) { return NGX_ERROR; }

    amcf = ngx_stream_get_module_main_conf(s, ngx_stream_accounting_module);

    metrics = ngx_traffic_accounting_period_fetch_metrics(amcf->current, accounting_id, amcf->log);
    if (metrics == NULL) { return NGX_ERROR; }

    if (ngx_traffic_accounting_metrics_init(metrics, ngx_stream_statuses_len, amcf->log) == NGX_ERROR)
        return NGX_ERROR;

    amcf->current->updated_at = ngx_timeofday();

    metrics->nr_entries += 1;
    metrics->bytes_in += s->received;
    metrics->bytes_out += s->connection->sent;

    if (s->status) {
        status = s->status;
    } else {
        status = NGX_STREAM_STATUS_UNSET;
    }

    metrics->nr_status[ngx_status_bsearch(status, ngx_stream_statuses, ngx_stream_statuses_len)] += 1;

    ms = (ngx_msec_int_t)((tp->sec - s->start_sec) * 1000 + (tp->msec - s->start_msec));
    ms = ngx_max(ms, 0);

    metrics->total_latency_ms += ms;

    if (s->upstream_states != NULL && s->upstream_states->nelts != 0) {
        ms = 0;
        state = s->upstream_states->elts;

        for (i = 0; i < s->upstream_states->nelts; i++) {
            ms += state[i].response_time;
        }
        ms = ngx_max(ms, 0);

        metrics->total_upstream_latency_ms += ms;
    }

    return NGX_DECLINED;
}

static ngx_stream_accounting_loc_conf_t *
ngx_stream_accounting_get_srv_conf(void *entry)
{
    return ngx_stream_get_module_srv_conf((ngx_stream_session_t *)entry, ngx_stream_accounting_module);
}

static ngx_stream_variable_value_t *
ngx_stream_accounting_get_indexed_variable(void *entry, ngx_uint_t index)
{
    return ngx_stream_get_indexed_variable((ngx_stream_session_t *)entry, index);
}

static ngx_str_t *
ngx_stream_accounting_get_accounting_id(ngx_stream_session_t *r)
{
    return ngx_traffic_accounting_get_accounting_id(r, ngx_stream_accounting_get_srv_conf, ngx_stream_accounting_get_indexed_variable);
}
