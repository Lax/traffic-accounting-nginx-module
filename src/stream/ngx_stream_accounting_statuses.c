
/*
 * Copyright (C) Liu Lantao
 */


#include "ngx_stream_accounting_module.h"


ngx_uint_t ngx_stream_statuses[] = {
    NGX_STREAM_STATUS_UNSET,

    NGX_STREAM_OK,
    NGX_STREAM_BAD_REQUEST,
    NGX_STREAM_FORBIDDEN,
    NGX_STREAM_INTERNAL_SERVER_ERROR,
    NGX_STREAM_BAD_GATEWAY,
    NGX_STREAM_SERVICE_UNAVAILABLE
};

ngx_uint_t ngx_stream_statuses_len = sizeof(ngx_stream_statuses)/sizeof(ngx_stream_statuses[0]);
