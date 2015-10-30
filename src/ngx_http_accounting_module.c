#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_accounting_hash.h"
#include "ngx_http_accounting_common.h"
#include "ngx_http_accounting_module.h"
#include "ngx_http_accounting_status_code.h"
#include "ngx_http_accounting_worker_process.h"


static ngx_int_t ngx_http_accounting_init(ngx_conf_t *cf);

static ngx_int_t ngx_http_accounting_process_init(ngx_cycle_t *cycle);
static void ngx_http_accounting_process_exit(ngx_cycle_t *cycle);

static void *ngx_http_accounting_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_accounting_init_main_conf(ngx_conf_t *cf, void *conf);

static void *ngx_http_accounting_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_accounting_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

static char *ngx_http_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t  ngx_http_accounting_commands[] = {
    { ngx_string("http_accounting"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_accounting_main_conf_t, enable),
      NULL},

    { ngx_string("http_accounting_id"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_accounting_set_accounting_id,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL},

    { ngx_string("http_accounting_log"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_accounting_main_conf_t, log),
      NULL},

    ngx_null_command
};


static ngx_http_module_t  ngx_http_accounting_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_http_accounting_init,               /* postconfiguration */
    ngx_http_accounting_create_main_conf,   /* create main configuration */
    ngx_http_accounting_init_main_conf,     /* init main configuration */
    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */
    ngx_http_accounting_create_loc_conf,    /* create location configuration */
    ngx_http_accounting_merge_loc_conf      /* merge location configuration */
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
    ngx_http_handler_pt              *h;
    ngx_http_core_main_conf_t        *cmcf;
    ngx_http_accounting_main_conf_t  *amcf;

    amcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_accounting_module);
    if (!amcf->enable) {
        return NGX_OK;
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_accounting_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_http_accounting_process_init(ngx_cycle_t *cycle)
{
    return ngx_http_accounting_worker_process_init(cycle);
}


static void
ngx_http_accounting_process_exit(ngx_cycle_t *cycle)
{
	return ngx_http_accounting_worker_process_exit(cycle);
}


static void *
ngx_http_accounting_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_accounting_main_conf_t  *amcf;

    amcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_accounting_main_conf_t));
    if (amcf == NULL) {
        return NULL;
    }

    amcf->enable = NGX_CONF_UNSET;

    return amcf;
}


static char *
ngx_http_accounting_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_accounting_main_conf_t *amcf = conf;

    if (amcf->enable == NGX_CONF_UNSET) {
        amcf->enable = 0;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_accounting_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_accounting_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_accounting_loc_conf_t));
    if(conf == NULL) {
        return NULL;
    }

    return conf;
}


static char *
ngx_http_accounting_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_accounting_loc_conf_t *prev = parent;
    ngx_http_accounting_loc_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->accounting_id, prev->accounting_id, "default");
    if (conf->index == 0) { // accounting_id is not set in current location
        conf->index = prev->index;
    }

    return NGX_CONF_OK;
}

static char *
ngx_http_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_accounting_loc_conf_t *alcf = conf;
    ngx_str_t                 *value;

    value = cf->args->elts;

    if (value[1].data[0] == '$') {
        value[1].len--;
        value[1].data++;

        alcf->index = ngx_http_get_variable_index(cf, &value[1]);
        if (alcf->index == NGX_ERROR) {
            return NGX_CONF_ERROR;
        }
        alcf->accounting_id = value[1];
        return NGX_CONF_OK;
    }

    alcf->accounting_id = value[1];
    alcf->index = DEFAULT_INDEX;

    return NGX_CONF_OK;
}