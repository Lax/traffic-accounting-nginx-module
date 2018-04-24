#ifndef _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_

#include <ngx_core.h>

/*
 * Log
 */

ngx_int_t ngxta_log_open(ngx_cycle_t *cycle, ngx_log_t *log, ngx_str_t *log_path);

#if (NGX_HAVE_VARIADIC_MACROS)
typedef void(ngxta_log_func_t)(ngx_uint_t level, ngx_log_t *log, ngx_err_t err, const char *fmt, ...);

void ngxta_log_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err, const char *fmt, ...);
#else
typedef void(ngxta_log_func_t)(ngx_uint_t level, ngx_log_t *log, ngx_err_t err, const char *fmt, va_list args)

void ngxta_log_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err, const char *fmt, va_list args)
#endif

extern ngx_log_t *ngxta_logger;
extern ngxta_log_func_t *ngxta_log;

#endif /* _NGX_TRAFFIC_ACCOUNTING_H_INCLUDED_ */
