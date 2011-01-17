#include <ngx_config.h>
#include <ngx_core.h>

#include "ngx_http_accounting_status_code.h"


ngx_uint_t http_status_code_to_index_map[512];
ngx_uint_t index_to_http_status_code_map[64];

ngx_uint_t http_status_code_count = 0;

void
init_http_status_code_map()
{
    ngx_uint_t i;

    for (i=0; i<sizeof(http_status_code_to_index_map)/sizeof(http_status_code_to_index_map[0]); i++)
        http_status_code_to_index_map[i] = -1;

    for (i=0; i<sizeof(index_to_http_status_code_map)/sizeof(index_to_http_status_code_map[0]); i++)
        index_to_http_status_code_map[i] = -1;

    i = 0;

    // Unknown
    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_UNKNOWN;
    http_status_code_to_index_map[NGX_HTTP_ACCT_UNKNOWN] = i++;

    // Informational 1xx
    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_CONTINUE;
    http_status_code_to_index_map[NGX_HTTP_ACCT_CONTINUE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_SWITCHING_PROTOCOLS;
    http_status_code_to_index_map[NGX_HTTP_ACCT_SWITCHING_PROTOCOLS] = i++;

    // Successful 2xx
    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_OK;
    http_status_code_to_index_map[NGX_HTTP_ACCT_OK] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_CREATED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_CREATED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_ACCEPTED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_ACCEPTED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_NON_AUTHORITATIVE_INFORMATION;
    http_status_code_to_index_map[NGX_HTTP_ACCT_NON_AUTHORITATIVE_INFORMATION] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_NO_CONTENT;
    http_status_code_to_index_map[NGX_HTTP_ACCT_NO_CONTENT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_RESET_CONTENT;
    http_status_code_to_index_map[NGX_HTTP_ACCT_RESET_CONTENT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_PARTIAL_CONTENT;
    http_status_code_to_index_map[NGX_HTTP_ACCT_PARTIAL_CONTENT] = i++;

    // Redirection 3xx
    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_MULTIPLE_CHOICES;
    http_status_code_to_index_map[NGX_HTTP_ACCT_MULTIPLE_CHOICES] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_MOVED_PERMANENTLY;
    http_status_code_to_index_map[NGX_HTTP_ACCT_MOVED_PERMANENTLY] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_FOUND;
    http_status_code_to_index_map[NGX_HTTP_ACCT_FOUND] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_SEE_OTHER;
    http_status_code_to_index_map[NGX_HTTP_ACCT_SEE_OTHER] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_NOT_MODIFIED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_NOT_MODIFIED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_USE_PROXY;
    http_status_code_to_index_map[NGX_HTTP_ACCT_USE_PROXY] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_UNUSED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_UNUSED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_TEMPORARY_REDIRECT;
    http_status_code_to_index_map[NGX_HTTP_ACCT_TEMPORARY_REDIRECT] = i++;

    // Client Error 4xx
    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_BAD_REQUEST;
    http_status_code_to_index_map[NGX_HTTP_ACCT_BAD_REQUEST] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_UNAUTHORIZED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_UNAUTHORIZED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_PAYMENT_REQUIRED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_PAYMENT_REQUIRED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_FORBIDDEN;
    http_status_code_to_index_map[NGX_HTTP_ACCT_FORBIDDEN] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_NOT_FOUND;
    http_status_code_to_index_map[NGX_HTTP_ACCT_NOT_FOUND] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_METHOD_NOT_ALLOWED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_METHOD_NOT_ALLOWED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_NOT_ACCEPTABLE;
    http_status_code_to_index_map[NGX_HTTP_ACCT_NOT_ACCEPTABLE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_PROXY_AUTHENTICATION_REQUIRED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_PROXY_AUTHENTICATION_REQUIRED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_REQUEST_TIMEOUT;
    http_status_code_to_index_map[NGX_HTTP_ACCT_REQUEST_TIMEOUT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_CONFLICT;
    http_status_code_to_index_map[NGX_HTTP_ACCT_CONFLICT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_GONE;
    http_status_code_to_index_map[NGX_HTTP_ACCT_GONE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_LENGTH_REQUIRED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_LENGTH_REQUIRED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_PRECONDITION_FAILED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_PRECONDITION_FAILED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_REQUEST_ENTITY_TOO_LARGE;
    http_status_code_to_index_map[NGX_HTTP_ACCT_REQUEST_ENTITY_TOO_LARGE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_REQUEST_URI_TOO_LONG;
    http_status_code_to_index_map[NGX_HTTP_ACCT_REQUEST_URI_TOO_LONG] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_UNSUPPORTED_MEDIA_TYPE;
    http_status_code_to_index_map[NGX_HTTP_ACCT_UNSUPPORTED_MEDIA_TYPE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_REQUESTED_RANGE_NOT_SATISFIABLE;
    http_status_code_to_index_map[NGX_HTTP_ACCT_REQUESTED_RANGE_NOT_SATISFIABLE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_EXPECTATION_FAILED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_EXPECTATION_FAILED] = i++;

    // Server Error 5xx
    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_INTERNAL_SERVER_ERROR;
    http_status_code_to_index_map[NGX_HTTP_ACCT_INTERNAL_SERVER_ERROR] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_NOT_IMPLEMENTED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_NOT_IMPLEMENTED] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_BAD_GATEWAY;
    http_status_code_to_index_map[NGX_HTTP_ACCT_BAD_GATEWAY] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_SERVICE_UNAVAILABLE;
    http_status_code_to_index_map[NGX_HTTP_ACCT_SERVICE_UNAVAILABLE] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_GATEWAY_TIMEOUT;
    http_status_code_to_index_map[NGX_HTTP_ACCT_GATEWAY_TIMEOUT] = i++;

    index_to_http_status_code_map[i] = NGX_HTTP_ACCT_HTTP_VERSION_NOT_SUPPORTED;
    http_status_code_to_index_map[NGX_HTTP_ACCT_HTTP_VERSION_NOT_SUPPORTED] = i++;

    http_status_code_count = i;
}
