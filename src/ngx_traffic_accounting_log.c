
/*
 * Copyright (C) Liu Lantao
 */


#include <syslog.h>
#include "ngx_traffic_accounting.h"


ngx_int_t
ngx_traffic_accounting_log_metrics(void *val, void *para1, void *para2,
    ngx_log_t *log, char entry_n[], ngx_uint_t statuses[], ngx_uint_t statuses_len)
{
    ngx_traffic_accounting_metrics_t   *metrics = (ngx_traffic_accounting_metrics_t *)val;
    ngx_time_t                         *created_at = para1;
    ngx_time_t                         *updated_at = para2;

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
        "pid:%i|from:%i|to:%i|accounting_id:%V|%s:%ui|bytes_in:%ui|bytes_out:%ui|latency_ms:%ui|upstream_latency_ms:%ui",
        ngx_getpid(),
        created_at->sec, updated_at->sec,
        &metrics->name, entry_n,
        metrics->nr_entries,
        metrics->bytes_in, metrics->bytes_out,
        metrics->total_latency_ms,
        metrics->total_upstream_latency_ms
    );

    for (i = 0; i < statuses_len; i++) {
        if (metrics->nr_status[i] == 0)
            continue;

        p = ngx_slprintf(p, last, "|%i:%i",
                    statuses[i], metrics->nr_status[i] );
    }

    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }

    *p++ = '\0';

    accounting_msg.len  = p - msg_buf;
    accounting_msg.data = msg_buf;

    if (log != NULL) {
        ngx_log_error(NGXTA_LOG_LEVEL, log, 0, "%V", &accounting_msg);
    } else {
        syslog(LOG_INFO, "%s", msg_buf);
    }

    return NGX_OK;
}
