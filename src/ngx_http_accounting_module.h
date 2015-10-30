#ifndef _NGX_HTTP_ACCOUNTING_MODULE_H_INCLUDED_
#define _NGX_HTTP_ACCOUNTING_MODULE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define DEFAULT_INDEX -128

typedef struct {
    ngx_str_t       accounting_id;
    ngx_int_t       index;
} ngx_http_accounting_loc_conf_t;

typedef struct {
    ngx_flag_t      enable;
    ngx_str_t       log;
} ngx_http_accounting_main_conf_t;

extern ngx_module_t ngx_http_accounting_module;

#endif /* _NGX_HTTP_ACCOUNTING_MODULE_H_INCLUDED_ */
