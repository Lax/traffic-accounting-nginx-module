
/*
 * Copyright (C) Liu Lantao
 */


#ifndef _NGX_TRAFFIC_ACCOUNTING_COMPAT_H_INCLUDED_
#define _NGX_TRAFFIC_ACCOUNTING_COMPAT_H_INCLUDED_

/*
 * All nginx-version-dependent adaptations live in this header.
 *
 * When adapting to a new nginx release, add the version guard here with
 * a comment naming the API/field and the nginx version that changed it,
 * and reference the macro from the code that uses it.  Policy and the
 * full supported-version window are documented in COMPATIBILITY.md.
 *
 * Must be included after ngx_core.h (nginx_version comes from nginx.h).
 */

#ifndef nginx_version
#error "include ngx_traffic_accounting_compat.h after ngx_core.h"
#endif

/*
 * ngx_http_upstream_state_t: the response_sec/response_msec pair was
 * replaced by the single response_time field (milliseconds) in
 * nginx 1.9.1 (module commit eb6f00c, "Nginx 1.9.1 support").
 */
#if (nginx_version >= 1009001)
#define NGXTA_HAVE_UPSTREAM_RESPONSE_TIME  1
#endif

#endif /* _NGX_TRAFFIC_ACCOUNTING_COMPAT_H_INCLUDED_ */
