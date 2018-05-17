
/*
 * Copyright (C) Liu Lantao
 */


#include "ngx_traffic_accounting.h"
#include "ngx_traffic_accounting_module.h"


void *
ngx_traffic_accounting_create_main_conf(ngx_conf_t *cf)
{
    ngx_traffic_accounting_main_conf_t   *amcf;

    amcf = ngx_pcalloc(cf->pool, sizeof(ngx_traffic_accounting_main_conf_t));
    if (amcf == NULL)
        return NULL;

    amcf->enable   = NGX_CONF_UNSET;
    amcf->interval = NGX_CONF_UNSET;
    amcf->perturb  = NGX_CONF_UNSET;

    return amcf;
}

char *
ngx_traffic_accounting_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_traffic_accounting_main_conf_t   *amcf = conf;

    if (amcf->enable   == NGX_CONF_UNSET) { amcf->enable   = 0;  }
    if (amcf->interval == NGX_CONF_UNSET) { amcf->interval = 60; }
    if (amcf->perturb  == NGX_CONF_UNSET) { amcf->perturb  = 0;  }

    return NGX_CONF_OK;
}

void *
ngx_traffic_accounting_create_loc_conf(ngx_conf_t *cf)
{
    ngx_traffic_accounting_loc_conf_t   *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_traffic_accounting_loc_conf_t));
    if(conf == NULL) { return NULL; }

    conf->index = NGX_CONF_UNSET;

    return conf;
}

char *
ngx_traffic_accounting_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_traffic_accounting_loc_conf_t   *prev = parent;
    ngx_traffic_accounting_loc_conf_t   *conf = child;

    if (conf->index == NGX_CONF_UNSET) { // accounting_id is not set in current location
        ngx_conf_merge_str_value(conf->accounting_id, prev->accounting_id, "default");
        conf->index = prev->index;
    }

    return NGX_CONF_OK;
}


char *
ngx_traffic_accounting_set_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_traffic_accounting_main_conf_t   *amcf = conf;
    char                *rc;
    ngx_log_t           *log;

    rc = ngx_log_set_log(cf, &amcf->log);
    if (rc != NGX_CONF_OK) { return rc; }

    log = amcf->log;
    while (log) {
        if (log->log_level < NGXTA_LOG_LEVEL) {
            log->log_level = NGXTA_LOG_LEVEL;
        }

        log = log->next;
    }

    return NGX_CONF_OK;
}

char *
ngx_traffic_accounting_set_accounting_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf,
    ngx_get_variable_index_pt get_variable_index)
{
    ngx_traffic_accounting_loc_conf_t   *alcf = conf;
    ngx_str_t                           *value;

    value = cf->args->elts;

    if (value[1].data[0] == '$') {
        value[1].len--;
        value[1].data++;

        alcf->index = get_variable_index(cf, &value[1]);
        if (alcf->index == NGX_ERROR) {
            return NGX_CONF_ERROR;
        }

        alcf->accounting_id = value[1];

        return NGX_CONF_OK;
    }

    alcf->accounting_id = value[1];
    alcf->index = NGX_CONF_INDEX_UNSET;

    return NGX_CONF_OK;
}



ngx_str_t *
ngx_traffic_accounting_get_accounting_id(void *entry, ngx_get_loc_conf_pt get_loc_conf,
    ngx_get_indexed_variable_pt get_indexed_variable)
{
    ngx_traffic_accounting_loc_conf_t   *alcf;
    ngx_variable_value_t                *vv;
    static ngx_str_t                     accounting_id;

    alcf = get_loc_conf(entry);
    if (alcf == NULL)
        return NULL;

    if (alcf->index != NGX_CONF_UNSET && alcf->index != NGX_CONF_INDEX_UNSET) {
        vv = get_indexed_variable(entry, alcf->index);

        if (vv != NULL) {
            if (vv->not_found) {
                vv->no_cacheable = 1;
                return NULL;
            }

            if (!vv->not_found) {
                // vv->data[vv->len] = '\0';

                accounting_id.len = vv->len;
                accounting_id.data = vv->data;

                return &accounting_id;
            }
        }
    }

    return &alcf->accounting_id;
}
