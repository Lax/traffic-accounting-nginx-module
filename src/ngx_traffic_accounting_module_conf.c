
/*
 * Copyright (C) Liu Lantao
 */


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
