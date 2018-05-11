
/*
 * Copyright (C) Liu Lantao
 */


#include <ngx_http.h>
#include <ngx_http_upstream.h>

#include <syslog.h>

#include "ngxta.h"
#include "ngx_http_accounting_module.h"
#include "ngx_http_accounting_worker_process.h"


static u_char *ngx_http_accounting_title = (u_char *)"NgxAccounting";

static void worker_process_alarm_handler(ngx_event_t *ev);
static ngx_str_t *get_accounting_id(ngx_http_request_t *r);


ngx_int_t
ngx_http_accounting_worker_process_init(ngx_cycle_t *cycle)
{
    ngx_http_accounting_main_conf_t *amcf;
    static ngx_event_t worker_process_alarm_event;

    amcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_accounting_module);
    if (!amcf->enable) {
        return NGX_OK;
    }

    ngxta_period_init(cycle->pool);

    if (ngxta_log.file->fd != NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_NOTICE, &ngxta_log, 0, "pid:%i|worker process start accounting", ngx_getpid());
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

    if (ngxta_log.file->fd != NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_NOTICE, &ngxta_log, 0, "pid:%i|worker process stop accounting", ngx_getpid());
    } else {
        syslog(LOG_INFO, "pid:%i|worker process stop accounting", ngx_getpid());
    }
}

ngx_int_t
ngx_http_accounting_handler(ngx_http_request_t *r)
{
    ngx_str_t      *accounting_id;

    ngx_uint_t      status;

    ngx_time_t * time = ngx_timeofday();

    accounting_id = get_accounting_id(r);
    if (accounting_id == NULL)
        return NGX_ERROR;

    ngxta_metrics_rbnode_t *metrics = ngxta_period_rbtree_lookup_metrics(ngxta_current_metrics, accounting_id);

    if (metrics == NULL) {
        ngxta_period_rbtree_insert(ngxta_current_metrics, accounting_id);

        metrics = ngxta_period_rbtree_lookup_metrics(ngxta_current_metrics, accounting_id);
        if (metrics == NULL)
            return NGX_ERROR;

        if (ngx_rstrncmp(accounting_id->data, metrics->name.data, accounting_id->len) != 0)
            return NGX_ERROR;

        metrics->nr_statuses = ngx_pcalloc(ngxta_current_metrics->pool,
                                   sizeof(ngx_uint_t) * ngxta_http_statuses_len);
        if (metrics->nr_statuses == NULL)
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

    metrics->nr_statuses[ngxta_statuses_bsearch(ngxta_http_statuses, ngxta_http_statuses_len, status)] += 1;

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

    ngxta_current_metrics->updated_at = ngx_timeofday();

    return NGX_DECLINED;
}


static ngx_int_t
worker_process_export_metrics(void *val, void *para1, void *para2)
{
    ngxta_metrics_rbnode_t  *metrics = (ngxta_metrics_rbnode_t *)val;
    ngx_time_t *created_at = para1;
    ngx_time_t *updated_at = para2;

    ngx_str_t     accounting_msg;
    u_char        msg_buf[NGX_MAX_ERROR_STR];
    u_char       *p, *last;
    ngx_uint_t    i;

    if (metrics->nr_entries == 0) {
        return NGX_OK;
    }

    if (metrics->name.len > 255) {
        // Possible buffer overrun. Do not log it.
        return NGX_OK;
    }

    // Output message buffer
    p = msg_buf;
    last = msg_buf + NGX_MAX_ERROR_STR;

    p = ngx_slprintf(p, last,
                "pid:%i|from:%i|to:%i|accounting_id:%s|requests:%ui|bytes_in:%ui|bytes_out:%ui|latency_ms:%ui|upstream_latency_ms:%ui",
                ngx_getpid(),
                created_at->sec, updated_at->sec,
                metrics->name.data,
                metrics->nr_entries,
                metrics->bytes_in,
                metrics->bytes_out,
                metrics->total_latency_ms,
                metrics->total_upstream_latency_ms
            );

    for (i = 0; i < ngxta_http_statuses_len; i++) {
        if (metrics->nr_statuses[i] == 0)
            continue;

        p = ngx_slprintf(p, last, "|%i:%i",
                    ngxta_http_statuses[i],
                    metrics->nr_statuses[i] );
    }

    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }

    *p++ = '\0';

    accounting_msg.len  = p - msg_buf;
    accounting_msg.data = msg_buf;

    if (ngxta_log.file->fd != NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_NOTICE, &ngxta_log, 0, "%V", &accounting_msg);
    } else {
        syslog(LOG_INFO, "%s", msg_buf);
    }

    return NGX_OK;
}

static void
worker_process_alarm_handler(ngx_event_t *ev)
{
    ngx_http_accounting_main_conf_t *amcf;

    ngxta_period_rotate(ngxta_current_metrics->pool);
    ngxta_period_rbtree_iterate(ngxta_previous_metrics,
                              worker_process_export_metrics,
                              ngxta_previous_metrics->created_at,
                              ngxta_previous_metrics->updated_at );

    if (ngx_exiting || ev == NULL)
        return;

    amcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_accounting_module);

    ngx_add_timer(ev, (ngx_msec_t)amcf->interval * 1000);
}


static ngx_str_t *
get_accounting_id(ngx_http_request_t *r)
{
    ngx_http_accounting_loc_conf_t  *alcf;
    ngx_http_variable_value_t       *vv;
    static ngx_str_t accounting_id;

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_accounting_module);
    if (alcf == NULL)
        return NULL;

    if (alcf->index != NGX_CONF_UNSET &&
        alcf->index != NGXTA_CONF_INDEX_UNSET) {
        vv = ngx_http_get_indexed_variable(r, alcf->index);

        if ((vv != NULL) && (vv->not_found)) {
            vv->no_cacheable = 1;
            return NULL;
        }

        if ((vv != NULL) && (!vv->not_found)) {
            accounting_id.len = vv->len;
            accounting_id.data = vv->data;
            return &accounting_id;
        }
    }

    return &alcf->accounting_id;
}
