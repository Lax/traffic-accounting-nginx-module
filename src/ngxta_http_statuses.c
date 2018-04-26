#include "ngxta.h"

#include <ngx_http.h>

#define NGX_HTTP_MAX_STATUS NGX_HTTP_INSUFFICIENT_STORAGE

ngx_uint_t ngxta_http_statuses[] = {
    NGX_HTTP_STATUS_UNSET,
    NGX_HTTP_CONTINUE,
    NGX_HTTP_SWITCHING_PROTOCOLS,
    NGX_HTTP_PROCESSING,
    NGX_HTTP_OK,
    NGX_HTTP_CREATED,
    NGX_HTTP_ACCEPTED,
    NGX_HTTP_NO_CONTENT,
    NGX_HTTP_PARTIAL_CONTENT,
    NGX_HTTP_SPECIAL_RESPONSE,
    NGX_HTTP_MOVED_PERMANENTLY,
    NGX_HTTP_MOVED_TEMPORARILY,
    NGX_HTTP_SEE_OTHER,
    NGX_HTTP_NOT_MODIFIED,
    NGX_HTTP_TEMPORARY_REDIRECT,
    NGX_HTTP_PERMANENT_REDIRECT,
    NGX_HTTP_BAD_REQUEST,
    NGX_HTTP_UNAUTHORIZED,
    NGX_HTTP_FORBIDDEN,
    NGX_HTTP_NOT_FOUND,
    NGX_HTTP_NOT_ALLOWED,
    NGX_HTTP_REQUEST_TIME_OUT,
    NGX_HTTP_CONFLICT,
    NGX_HTTP_LENGTH_REQUIRED,
    NGX_HTTP_PRECONDITION_FAILED,
    NGX_HTTP_REQUEST_ENTITY_TOO_LARGE,
    NGX_HTTP_REQUEST_URI_TOO_LARGE,
    NGX_HTTP_UNSUPPORTED_MEDIA_TYPE,
    NGX_HTTP_RANGE_NOT_SATISFIABLE,
    NGX_HTTP_MISDIRECTED_REQUEST,
    NGX_HTTP_TOO_MANY_REQUESTS,
    NGX_HTTP_CLOSE,
    NGX_HTTP_NGINX_CODES,
    NGX_HTTP_REQUEST_HEADER_TOO_LARGE,
    NGX_HTTPS_CERT_ERROR,
    NGX_HTTPS_NO_CERT,
    NGX_HTTP_TO_HTTPS,
    NGX_HTTP_CLIENT_CLOSED_REQUEST,
    NGX_HTTP_INTERNAL_SERVER_ERROR,
    NGX_HTTP_NOT_IMPLEMENTED,
    NGX_HTTP_BAD_GATEWAY,
    NGX_HTTP_SERVICE_UNAVAILABLE,
    NGX_HTTP_GATEWAY_TIME_OUT,
    NGX_HTTP_VERSION_NOT_SUPPORTED,
    NGX_HTTP_INSUFFICIENT_STORAGE
};

static int statuses_cmp(const void * a, const void * b);

ngx_uint_t
ngxta_statuses_count(ngx_uint_t statuses[])
{
    return sizeof(statuses) / sizeof(statuses[0]);
}

ngx_uint_t
ngxta_statuses_bsearch(ngx_uint_t statuses[], ngx_uint_t *status)
{
    ngx_uint_t *match;

    match = bsearch(status, statuses, ngxta_statuses_count(statuses), sizeof(ngx_uint_t), statuses_cmp);
    if ( match == NULL ) {
        return 0;
    }

    return ((ngx_uint_t *)match - statuses);
}

static int
statuses_cmp(const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}
