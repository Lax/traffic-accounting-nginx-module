
/*
 * Copyright (C) Liu Lantao
 */


#include "ngx_traffic_accounting.h"
#include "ngx_traffic_accounting_module.h"


ngx_int_t
ngx_traffic_accounting_period_create(ngx_traffic_accounting_main_conf_t *amcf)
{
    ngx_traffic_accounting_period_t   *period;

    period = ngx_calloc(sizeof(ngx_traffic_accounting_period_t), amcf->log);
    if (period == NULL)
        return NGX_ERROR;

    ngx_traffic_accounting_period_init(period);

    period->created_at = ngx_timeofday();

    amcf->current = period;

    return NGX_OK;
}

ngx_int_t
ngx_traffic_accounting_period_rotate(ngx_traffic_accounting_main_conf_t *amcf)
{
    ngx_free(amcf->previous);

    amcf->previous = amcf->current;

    return ngx_traffic_accounting_period_create(amcf);
}
