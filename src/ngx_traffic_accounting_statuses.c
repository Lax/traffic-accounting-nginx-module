
/*
 * Copyright (C) Liu Lantao
 */


#include "ngx_traffic_accounting.h"


static int statuses_cmp(const void * a, const void * b);

ngx_uint_t
ngx_status_bsearch(ngx_uint_t status, ngx_uint_t statuses[], ngx_uint_t size)
{
    ngx_uint_t *match;

    match = bsearch(&status, statuses, size, sizeof(ngx_uint_t), statuses_cmp);
    if ( match == NULL ) {
        return 0;
    }

    return ((ngx_uint_t *)match - statuses);
}

static int
statuses_cmp(const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}
