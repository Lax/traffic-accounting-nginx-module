#ifndef _NGX_HTTP_ACCOUNTING_COMMON_H_INCLUDED_
#define _NGX_HTTP_ACCOUNTING_COMMON_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>


#define ACCOUNTING_ID_MAX_LEN               10
#define NGX_HTTP_ACCOUNTING_NR_BUCKETS      107

typedef struct {
    ngx_uint_t       nr_requests;
    ngx_uint_t       bytes_in;
    ngx_uint_t       bytes_out;
    ngx_uint_t       total_latency_ms;
    ngx_uint_t       total_upstream_latency_ms;
    ngx_uint_t      *http_status_code;
} ngx_http_accounting_stats_t;

#endif /* _NGX_HTTP_ACCOUNTING_COMMON_H_INCLUDED_ */
