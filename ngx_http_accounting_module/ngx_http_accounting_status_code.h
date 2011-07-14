#ifndef _NGX_HTTP_ACCOUNTING_STATUS_CODE_H_INCLUDED_
#define _NGX_HTTP_ACCOUNTING_STATUS_CODE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>

// Default 0
#define NGX_HTTP_DEFAULT                                0

// Informational 1xx
//#define NGX_HTTP_ACCT_CONTINUE                          100
//#define NGX_HTTP_ACCT_SWITCHING_PROTOCOLS               101

// Successful 2xx
//#define NGX_HTTP_ACCT_OK                                200
//#define NGX_HTTP_ACCT_CREATED                           201
//#define NGX_HTTP_ACCT_ACCEPTED                          202
//#define NGX_HTTP_ACCT_NON_AUTHORITATIVE_INFORMATION     203
//#define NGX_HTTP_ACCT_NO_CONTENT                        204
//#define NGX_HTTP_ACCT_RESET_CONTENT                     205
//#define NGX_HTTP_ACCT_PARTIAL_CONTENT                   206

// Redirection 3xx
//#define NGX_HTTP_ACCT_MULTIPLE_CHOICES                  300
//#define NGX_HTTP_ACCT_MOVED_PERMANENTLY                 301
//#define NGX_HTTP_ACCT_FOUND                             302
//#define NGX_HTTP_ACCT_SEE_OTHER                         303
//#define NGX_HTTP_ACCT_NOT_MODIFIED                      304
//#define NGX_HTTP_ACCT_USE_PROXY                         305
//#define NGX_HTTP_ACCT_UNUSED                            306
//#define NGX_HTTP_ACCT_TEMPORARY_REDIRECT                307

// Client Error 4xx
//#define NGX_HTTP_ACCT_BAD_REQUEST                       400
//#define NGX_HTTP_ACCT_UNAUTHORIZED                      401
//#define NGX_HTTP_ACCT_PAYMENT_REQUIRED                  402
//#define NGX_HTTP_ACCT_FORBIDDEN                         403
//#define NGX_HTTP_ACCT_NOT_FOUND                         404
//#define NGX_HTTP_ACCT_METHOD_NOT_ALLOWED                405
//#define NGX_HTTP_ACCT_NOT_ACCEPTABLE                    406
//#define NGX_HTTP_ACCT_PROXY_AUTHENTICATION_REQUIRED     407
//#define NGX_HTTP_ACCT_REQUEST_TIMEOUT                   408
//#define NGX_HTTP_ACCT_CONFLICT                          409
//#define NGX_HTTP_ACCT_GONE                              410
//#define NGX_HTTP_ACCT_LENGTH_REQUIRED                   411
//#define NGX_HTTP_ACCT_PRECONDITION_FAILED               412
//#define NGX_HTTP_ACCT_REQUEST_ENTITY_TOO_LARGE          413
//#define NGX_HTTP_ACCT_REQUEST_URI_TOO_LONG              414
//#define NGX_HTTP_ACCT_UNSUPPORTED_MEDIA_TYPE            415
//#define NGX_HTTP_ACCT_REQUESTED_RANGE_NOT_SATISFIABLE   416
//#define NGX_HTTP_ACCT_EXPECTATION_FAILED                417

// Server Error 5xx
//#define NGX_HTTP_ACCT_INTERNAL_SERVER_ERROR             500
//#define NGX_HTTP_ACCT_NOT_IMPLEMENTED                   501
//#define NGX_HTTP_ACCT_BAD_GATEWAY                       502
//#define NGX_HTTP_ACCT_SERVICE_UNAVAILABLE               503
//#define NGX_HTTP_ACCT_GATEWAY_TIMEOUT                   504
//#define NGX_HTTP_ACCT_HTTP_VERSION_NOT_SUPPORTED        505

extern ngx_uint_t http_status_code_to_index_map[];
extern ngx_uint_t index_to_http_status_code_map[];

extern ngx_uint_t http_status_code_count;

void init_http_status_code_map();

#endif /* _NGX_HTTP_ACCOUNTING_STATUS_CODE_H_INCLUDED_ */
