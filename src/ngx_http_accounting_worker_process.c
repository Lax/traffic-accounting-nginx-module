#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_upstream.h>
#include <nginx.h>

#include <syslog.h>

#include "ngxta.h"
#include "ngx_http_accounting_hash.h"
#include "ngx_http_accounting_module.h"
#include "ngx_http_accounting_common.h"
#include "ngx_http_accounting_status_code.h"
#include "ngx_http_accounting_worker_process.h"

static time_t worker_process_timer_interval;    /* In seconds */
static ngx_flag_t worker_process_timer_perturb;

static ngx_event_t  write_out_ev;
static ngx_http_accounting_hash_t  stats_hash;

static ngx_int_t ngx_http_accounting_old_time = 0;
static ngx_int_t ngx_http_accounting_new_time = 0;

static u_char *ngx_http_accounting_title = (u_char *)"NgxAccounting";

static void worker_process_alarm_handler(ngx_event_t *ev);
static ngx_str_t *get_accounting_id(ngx_http_request_t *r);


ngx_int_t
ngx_http_accounting_worker_process_init(ngx_cycle_t *cycle)
{
    ngx_int_t rc;
    ngx_time_t  *time;
    ngx_http_accounting_main_conf_t *amcf;

    amcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_accounting_module);

    if (!amcf->enable) {
        return NGX_OK;
    }

    init_http_status_code_map();

    time = ngx_timeofday();

    ngx_http_accounting_old_time = time->sec;
    ngx_http_accounting_new_time = time->sec;
    worker_process_timer_interval = amcf->interval;
    worker_process_timer_perturb = amcf->perturb;

    if (ngxta_log.file->fd != NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_NOTICE, &ngxta_log, 0, "pid:%i|worker process start accounting", ngx_getpid());
    } else {
        openlog((char *)ngx_http_accounting_title, LOG_NDELAY, LOG_SYSLOG);
        syslog(LOG_INFO, "pid:%i|worker process start accounting", ngx_getpid());
    }

    rc = ngx_http_accounting_hash_init(&stats_hash, NGX_HTTP_ACCOUNTING_NR_BUCKETS, cycle->pool);
    if (rc != NGX_OK) {
        return rc;
    }

    ngx_memzero(&write_out_ev, sizeof(ngx_event_t));

    write_out_ev.data = NULL;
    write_out_ev.log = cycle->log;
    write_out_ev.handler = worker_process_alarm_handler;
    write_out_ev.cancelable = 1;

    srand(ngx_getpid());
    time_t perturb_factor = 1;
    if (worker_process_timer_perturb) {
        perturb_factor = (1000-rand()%200);
    }

    ngx_add_timer(&write_out_ev, worker_process_timer_interval * perturb_factor);

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
    ngx_uint_t      key;

    ngx_uint_t      status;
    ngx_uint_t     *status_array;


    ngx_http_accounting_stats_t *stats;

    ngx_time_t * time = ngx_timeofday();

    accounting_id = get_accounting_id(r);

    ngx_uint_t req_latency_ms = (time->sec * 1000 + time->msec) - (r->start_sec * 1000 + r->start_msec);

    // following magic airlifted from ngx_http_upstream.c:4416-4423
    ngx_uint_t upstream_req_latency_ms = 0;
    ngx_http_upstream_state_t  *state;

    if (r->upstream_states != NULL && r->upstream_states->nelts != 0) {
        state = r->upstream_states->elts;
        if (state[0].status) {
            // not even checking the status here...
#if (nginx_version < 1009000)
            upstream_req_latency_ms = (state[0].response_sec * 1000 + state[0].response_msec);
#else
            upstream_req_latency_ms = state[0].response_time;
#endif
        }
    }
    // TODO: key should be cached to save CPU time
    key = ngx_hash_key_lc(accounting_id->data, accounting_id->len);
    stats = ngx_http_accounting_hash_find(&stats_hash, key, accounting_id->data, accounting_id->len);

    if (stats == NULL) {

        stats = ngx_pcalloc(stats_hash.pool, sizeof(ngx_http_accounting_stats_t));
        status_array = ngx_pcalloc(stats_hash.pool, sizeof(ngx_uint_t) * http_status_code_count);

        if (stats == NULL || status_array == NULL)
            return NGX_ERROR;

        stats->http_status_code = status_array;
        ngx_http_accounting_hash_add(&stats_hash, key, accounting_id->data, accounting_id->len, stats);
    }

    if (r->err_status) {
        status = r->err_status;
    } else if (r->headers_out.status) {
        status = r->headers_out.status;
    } else {
        status = NGX_HTTP_DEFAULT;
    }

    stats->nr_requests += 1;
    stats->bytes_in += r->request_length;
    stats->bytes_out += r->connection->sent;
    stats->total_latency_ms += req_latency_ms;
    stats->total_upstream_latency_ms += upstream_req_latency_ms;
    stats->http_status_code[http_status_code_to_index_map[status]] += 1;

    return NGX_OK;
}


static ngx_int_t
worker_process_write_out_stats(u_char *name, size_t len, void *val, void *para1, void *para2)
{
    ngx_uint_t    i;
    ngx_http_accounting_stats_t  *stats;

    ngx_str_t     accounting_msg;
    u_char        msg_buf[NGX_MAX_ERROR_STR];
    u_char       *p, *last;

    stats = (ngx_http_accounting_stats_t *)val;

    if (stats->nr_requests == 0) {
        return NGX_OK;
    }

    if (ngx_strlen(name) > 255) {
        // Possible buffer overrun. Do not log it.
        stats->nr_requests = 0;
        stats->bytes_in = 0;
        stats->bytes_out = 0;
        stats->total_latency_ms = 0;
        stats->total_upstream_latency_ms = 0;
        return NGX_OK;
    }

    // Output message buffer
    p = msg_buf;
    last = msg_buf + NGX_MAX_ERROR_STR;

    p = ngx_slprintf(p, last,
                "pid:%i|from:%i|to:%i|accounting_id:%s|requests:%ui|bytes_in:%ui|bytes_out:%ui|latency_ms:%ui|upstream_latency_ms:%ui",
                ngx_getpid(),
                ngx_http_accounting_old_time,
                ngx_http_accounting_new_time,
                name,
                stats->nr_requests,
                stats->bytes_in,
                stats->bytes_out,
                stats->total_latency_ms,
                stats->total_upstream_latency_ms
            );

    stats->nr_requests = 0;
    stats->bytes_in = 0;
    stats->bytes_out = 0;
    stats->total_latency_ms = 0;
    stats->total_upstream_latency_ms = 0;

    for (i = 0; i < http_status_code_count; i++) {
        if(stats->http_status_code[i] > 0) {

            p = ngx_slprintf(p, last, "|%i:%i",
                        index_to_http_status_code_map[i],
                        stats->http_status_code[i] );

            stats->http_status_code[i] = 0;
        }
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
    ngx_time_t  *time;
    ngx_msec_t   next;

    time = ngx_timeofday();

    ngx_http_accounting_old_time = ngx_http_accounting_new_time;
    ngx_http_accounting_new_time = time->sec;

    ngx_http_accounting_hash_iterate(&stats_hash, worker_process_write_out_stats, NULL, NULL);

    if (ngx_exiting || ev == NULL)
        return;

    next = (ngx_msec_t)worker_process_timer_interval * 1000;

    ngx_add_timer(ev, next);
}


static ngx_str_t *
get_accounting_id(ngx_http_request_t *r)
{
    ngx_http_accounting_loc_conf_t  *alcf;
    ngx_http_variable_value_t       *vv;
    static ngx_str_t accounting_id;

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_accounting_module);

    if (alcf->index > 0) {
        vv = ngx_http_get_indexed_variable(r, alcf->index);

        if ((vv != NULL) && (!vv->not_found)) {
            accounting_id.len = vv->len;
            accounting_id.data = vv->data;
            return &accounting_id;
        }
    }

    return &alcf->accounting_id;
}
