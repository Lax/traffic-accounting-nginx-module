
/*
 * Copyright (C) Liu Lantao
 */


#include "ngx_traffic_accounting.h"
#include "ngx_traffic_accounting_module.h"

#include <time.h>


void *
ngx_traffic_accounting_create_main_conf(ngx_conf_t *cf)
{
    ngx_traffic_accounting_main_conf_t   *amcf;

    amcf = ngx_pcalloc(cf->pool, sizeof(ngx_traffic_accounting_main_conf_t));
    if (amcf == NULL)
        return NULL;

    amcf->enable       = NGX_CONF_UNSET;
    amcf->interval     = NGX_CONF_UNSET;
    amcf->perturb      = NGX_CONF_UNSET;
    amcf->period_reset = NGX_TA_PERIOD_RESET_NONE;
    amcf->last_reset_day = 0;

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
    conf->skip  = NGX_CONF_UNSET;

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

    ngx_conf_merge_value(conf->skip, prev->skip, 0);

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
ngx_traffic_accounting_set_period_reset(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_traffic_accounting_main_conf_t   *amcf = conf;
    ngx_str_t                            *value;

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "none") == 0) {
        amcf->period_reset = NGX_TA_PERIOD_RESET_NONE;
    } else if (ngx_strcmp(value[1].data, "hourly") == 0) {
        amcf->period_reset = NGX_TA_PERIOD_RESET_HOURLY;
    } else if (ngx_strcmp(value[1].data, "daily") == 0) {
        amcf->period_reset = NGX_TA_PERIOD_RESET_DAILY;
    } else if (ngx_strcmp(value[1].data, "weekly") == 0) {
        amcf->period_reset = NGX_TA_PERIOD_RESET_WEEKLY;
    } else if (ngx_strcmp(value[1].data, "monthly") == 0) {
        amcf->period_reset = NGX_TA_PERIOD_RESET_MONTHLY;
    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid value \"%V\", must be: "
                           "none | hourly | daily | weekly | monthly",
                           &value[1]);
        return NGX_CONF_ERROR;
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


ngx_int_t
ngx_traffic_accounting_check_reset(ngx_traffic_accounting_main_conf_t *amcf)
{
    ngx_time_t *tp;
    time_t      now;
    struct tm   tm;

    if (amcf->period_reset == NGX_TA_PERIOD_RESET_NONE) {
        return 0;
    }

    tp = ngx_timeofday();
    now = tp->sec;
    localtime_r(&now, &tm);

    switch (amcf->period_reset) {
    case NGX_TA_PERIOD_RESET_HOURLY:
        if ((ngx_uint_t)tm.tm_hour != amcf->last_reset_day) {
            amcf->last_reset_day = tm.tm_hour;
            return 1;
        }
        break;
    case NGX_TA_PERIOD_RESET_DAILY:
        if ((ngx_uint_t)tm.tm_mday != amcf->last_reset_day) {
            amcf->last_reset_day = tm.tm_mday;
            return 1;
        }
        break;
    case NGX_TA_PERIOD_RESET_WEEKLY:
        if (tm.tm_wday == 1
            && (ngx_uint_t)tm.tm_yday != amcf->last_reset_day)
        {
            amcf->last_reset_day = tm.tm_yday;
            return 1;
        }
        break;
    case NGX_TA_PERIOD_RESET_MONTHLY:
        if ((ngx_uint_t)tm.tm_mon != amcf->last_reset_day) {
            amcf->last_reset_day = tm.tm_mon;
            return 1;
        }
        break;
    default:
        break;
    }

    return 0;
}
