
/*
 * Copyright (C) Liu Lantao
 */


#include "ngx_traffic_accounting.h"
#include "ngx_traffic_accounting_module.h"


ngx_traffic_accounting_period_t *
ngx_traffic_accounting_period_create(ngx_traffic_accounting_main_conf_t *amcf)
{
    ngx_traffic_accounting_period_t   *period;

    period = ngx_calloc(sizeof(ngx_traffic_accounting_period_t), amcf->log);
    if (period == NULL) {
        return NULL;
    }

    ngx_traffic_accounting_period_init(period);

    period->created_at = *ngx_timeofday();
    period->updated_at = period->created_at;

    return period;
}
