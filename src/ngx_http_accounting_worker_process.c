
/*
 * Copyright (C) Liu Lantao
 */


#include <ngx_http.h>
#include <ngx_http_upstream.h>

#include <syslog.h>

#include "ngx_traffic_accounting.h"
#include "ngx_http_accounting_module.h"


static char entry_n[] = "requests";

static u_char *ngx_http_accounting_title = (u_char *)"NgxAccounting";

static void worker_process_alarm_handler(ngx_event_t *ev);
static ngx_str_t *ngx_http_accounting_get_accounting_id(ngx_http_request_t *r);


ngx_int_t
ngx_http_accounting_worker_process_init(ngx_cycle_t *cycle)
{
    ngx_http_accounting_main_conf_t   *amcf;
    static ngx_event_t worker_process_alarm_event;

    amcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_accounting_module);
    if (!amcf->enable) {
        return NGX_OK;
    }

    if (amcf->current == NULL)
        ngx_traffic_accounting_period_create(cycle->pool, amcf);

    if (amcf->log != NULL) {
        ngx_log_error(NGXTA_LOG_LEVEL, amcf->log, 0, "pid:%i|worker process start accounting", ngx_getpid());
    } else {
        openlog((char *)ngx_http_accounting_title, LOG_NDELAY, LOG_SYSLOG);
        syslog(LOG_INFO, "pid:%i|worker process start accounting", ngx_getpid());
    }

    ngx_memzero(&worker_process_alarm_event, sizeof(ngx_event_t));

    worker_process_alarm_event.data = NULL;
    worker_process_alarm_event.log = cycle->log;
    worker_process_alarm_event.handler = worker_process_alarm_handler;
    worker_process_alarm_event.cancelable = 1;

    time_t perturb_factor = 1000;
    if (amcf->perturb) {
        srand(ngx_getpid());
        perturb_factor = (1000-rand()%200);
    }

    ngx_add_timer(&worker_process_alarm_event, amcf->interval * perturb_factor);

    return NGX_OK;
}


void ngx_http_accounting_worker_process_exit(ngx_cycle_t *cycle)
{
    ngx_http_accounting_main_conf_t *amcf;

    amcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_accounting_module);

    if (!amcf->enable) {
        return;
    }

    worker_process_alarm_handler(NULL);

    if (amcf->log != NULL) {
        ngx_log_error(NGXTA_LOG_LEVEL, amcf->log, 0, "pid:%i|worker process stop accounting", ngx_getpid());
    } else {
        syslog(LOG_INFO, "pid:%i|worker process stop accounting", ngx_getpid());
    }
}

ngx_int_t
ngx_http_accounting_handler(ngx_http_request_t *r)
{
    ngx_str_t                          *accounting_id;
    ngx_traffic_accounting_metrics_t   *metrics;
    ngx_http_accounting_main_conf_t    *amcf;

    ngx_uint_t      status;
    ngx_time_t * time = ngx_timeofday();

    accounting_id = ngx_http_accounting_get_accounting_id(r);
    if (accounting_id == NULL) { return NGX_ERROR; }

    amcf = ngx_http_get_module_main_conf(r, ngx_http_accounting_module);

    metrics = ngx_traffic_accounting_period_fetch_metrics(amcf->current, accounting_id);
    if (metrics == NULL) { return NGX_ERROR; }

    if (ngx_traffic_accounting_metrics_init(metrics, amcf->current->pool, ngx_http_statuses_len) == NGX_ERROR)
        return NGX_ERROR;

    amcf->current->updated_at = ngx_timeofday();

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

    metrics->total_latency_ms +=
        (time->sec * 1000 + time->msec) - (r->start_sec * 1000 + r->start_msec);

    if (r->upstream_states != NULL && r->upstream_states->nelts != 0) {
        ngx_uint_t                   upstream_req_latency_ms = 0;
        ngx_uint_t                   i;
        ngx_http_upstream_state_t   *state = r->upstream_states->elts;

        for (i = 0; i < r->upstream_states->nelts; i++) {
            if (state[i].status) {
#if (nginx_version < 1009000)
                upstream_req_latency_ms += (state[i].response_sec * 1000 + state[i].response_msec);
#else
                upstream_req_latency_ms += state[i].response_time;
#endif
            }

        }

        metrics->total_upstream_latency_ms += upstream_req_latency_ms;
    }

    return NGX_DECLINED;
}


static ngx_int_t
worker_process_export_metrics(void *val, void *para1, void *para2)
{
    ngx_http_accounting_main_conf_t   *amcf;
    ngx_int_t                          rc;

    amcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_accounting_module);

    rc = ngx_traffic_accounting_log_metrics(val, para1, para2,
                                            amcf->log, entry_n,
                                            ngx_http_statuses,
                                            ngx_http_statuses_len );
    if (rc == NGX_OK) { return NGX_DONE; }  /* NGX_DONE -> destroy node */

    return rc;
}

static void
worker_process_alarm_handler(ngx_event_t *ev)
{
    ngx_http_accounting_main_conf_t *amcf;

    amcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_accounting_module);

    ngx_traffic_accounting_period_rotate(amcf->current->pool, amcf);
    ngx_traffic_accounting_period_rbtree_iterate(amcf->previous,
                              worker_process_export_metrics,
                              amcf->previous->created_at,
                              amcf->previous->updated_at );

    if (ngx_exiting || ev == NULL)
        return;

    ngx_add_timer(ev, (ngx_msec_t)amcf->interval * 1000);
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
