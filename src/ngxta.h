#ifndef _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_

#include <ngx_core.h>

/*
 * Log
 */

ngx_int_t ngxta_log_open(ngx_cycle_t *cycle, ngx_log_t *log, ngx_str_t *log_path);

extern ngx_log_t ngxta_log;

#endif /* _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_ */
