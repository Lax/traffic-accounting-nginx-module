
/*
 * Copyright (C) Liu Lantao
 */


#include "ngx_traffic_accounting.h"
#include "ngx_traffic_accounting_module.h"


ngx_int_t
ngx_traffic_accounting_period_create(ngx_pool_t *pool, ngx_traffic_accounting_main_conf_t *amcf)
{
    ngx_traffic_accounting_period_t   *period;

    period = ngx_pcalloc(pool, sizeof(ngx_traffic_accounting_period_t));
    if (period == NULL)
        return NGX_ERROR;

    period->pool = pool;
    ngx_traffic_accounting_period_init(period);

    period->created_at = ngx_timeofday();

    amcf->current = period;

    return NGX_OK;
}

ngx_int_t
ngx_traffic_accounting_period_rotate(ngx_pool_t *pool, ngx_traffic_accounting_main_conf_t *amcf)
{
    ngx_pfree(pool, amcf->previous);

    amcf->previous = amcf->current;

    return ngx_traffic_accounting_period_create(pool, amcf);
}
