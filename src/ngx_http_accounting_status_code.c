#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_accounting_status_code.h"


ngx_uint_t http_status_code_to_index_map[512];
ngx_uint_t index_to_http_status_code_map[64];

ngx_uint_t http_status_code_count = 0;

void
init_http_status_code_map()
{
    ngx_uint_t i;

    for (i=0; i<sizeof(http_status_code_to_index_map)/sizeof(http_status_code_to_index_map[0]); i++)
        http_status_code_to_index_map[i] = NGX_HTTP_DEFAULT;

    for (i=0; i<sizeof(index_to_http_status_code_map)/sizeof(index_to_http_status_code_map[0]); i++)
        index_to_http_status_code_map[i] = 0;

    i = 0;

    // Default
    index_to_http_status_code_map[i] = NGX_HTTP_DEFAULT;
    http_status_code_to_index_map[NGX_HTTP_DEFAULT] = i++;

    // Successful 2xx
    index_to_http_status_code_map[i] = NGX_HTTP_OK;
    http_status_code_to_index_map[NGX_HTTP_OK] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_CREATED;
    http_status_code_to_index_map[NGX_HTTP_CREATED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_NO_CONTENT;
    http_status_code_to_index_map[NGX_HTTP_NO_CONTENT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_PARTIAL_CONTENT;
    http_status_code_to_index_map[NGX_HTTP_PARTIAL_CONTENT] = i++;

    // Redirection 3xx
    index_to_http_status_code_map[i] = NGX_HTTP_SPECIAL_RESPONSE;
    http_status_code_to_index_map[NGX_HTTP_SPECIAL_RESPONSE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_MOVED_PERMANENTLY;
    http_status_code_to_index_map[NGX_HTTP_MOVED_PERMANENTLY] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_MOVED_TEMPORARILY;
    http_status_code_to_index_map[NGX_HTTP_MOVED_TEMPORARILY] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_NOT_MODIFIED;
    http_status_code_to_index_map[NGX_HTTP_NOT_MODIFIED] = i++;

    // Client Error 4xx
    index_to_http_status_code_map[i] = NGX_HTTP_BAD_REQUEST;
    http_status_code_to_index_map[NGX_HTTP_BAD_REQUEST] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_UNAUTHORIZED;
    http_status_code_to_index_map[NGX_HTTP_UNAUTHORIZED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_FORBIDDEN;
    http_status_code_to_index_map[NGX_HTTP_FORBIDDEN] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_NOT_FOUND;
    http_status_code_to_index_map[NGX_HTTP_NOT_FOUND] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_NOT_ALLOWED;
    http_status_code_to_index_map[NGX_HTTP_NOT_ALLOWED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_REQUEST_TIME_OUT;
    http_status_code_to_index_map[NGX_HTTP_REQUEST_TIME_OUT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_CONFLICT;
    http_status_code_to_index_map[NGX_HTTP_CONFLICT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_LENGTH_REQUIRED;
    http_status_code_to_index_map[NGX_HTTP_LENGTH_REQUIRED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_PRECONDITION_FAILED;
    http_status_code_to_index_map[NGX_HTTP_PRECONDITION_FAILED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_REQUEST_ENTITY_TOO_LARGE;
    http_status_code_to_index_map[NGX_HTTP_REQUEST_ENTITY_TOO_LARGE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_REQUEST_URI_TOO_LARGE;
    http_status_code_to_index_map[NGX_HTTP_REQUEST_URI_TOO_LARGE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
    http_status_code_to_index_map[NGX_HTTP_UNSUPPORTED_MEDIA_TYPE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_RANGE_NOT_SATISFIABLE;
    http_status_code_to_index_map[NGX_HTTP_RANGE_NOT_SATISFIABLE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_CLOSE;
    http_status_code_to_index_map[NGX_HTTP_CLOSE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_NGINX_CODES;
    http_status_code_to_index_map[NGX_HTTP_NGINX_CODES] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_REQUEST_HEADER_TOO_LARGE;
    http_status_code_to_index_map[NGX_HTTP_REQUEST_HEADER_TOO_LARGE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTPS_CERT_ERROR;
    http_status_code_to_index_map[NGX_HTTPS_CERT_ERROR] = i++;

    index_to_http_status_code_map[i] = NGX_HTTPS_NO_CERT;
    http_status_code_to_index_map[NGX_HTTPS_NO_CERT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_TO_HTTPS;
    http_status_code_to_index_map[NGX_HTTP_TO_HTTPS] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_CLIENT_CLOSED_REQUEST;
    http_status_code_to_index_map[NGX_HTTP_CLIENT_CLOSED_REQUEST] = i++;

    // Server Error 5xx
    index_to_http_status_code_map[i] = NGX_HTTP_INTERNAL_SERVER_ERROR;
    http_status_code_to_index_map[NGX_HTTP_INTERNAL_SERVER_ERROR] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_NOT_IMPLEMENTED;
    http_status_code_to_index_map[NGX_HTTP_NOT_IMPLEMENTED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_BAD_GATEWAY;
    http_status_code_to_index_map[NGX_HTTP_BAD_GATEWAY] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_SERVICE_UNAVAILABLE;
    http_status_code_to_index_map[NGX_HTTP_SERVICE_UNAVAILABLE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_GATEWAY_TIME_OUT;
    http_status_code_to_index_map[NGX_HTTP_GATEWAY_TIME_OUT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_INSUFFICIENT_STORAGE;
    http_status_code_to_index_map[NGX_HTTP_INSUFFICIENT_STORAGE] = i++;

    http_status_code_count = i;
}
