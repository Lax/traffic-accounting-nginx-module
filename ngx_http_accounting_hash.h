#ifndef _NGX_HTTP_ACCOUNTING_HASH_H_INCLUDED_
#define _NGX_HTTP_ACCOUNTING_HASH_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    ngx_array_t     **buckets;
    ngx_uint_t        size;
    ngx_pool_t       *pool;
} ngx_http_accounting_hash_t;

typedef ngx_int_t (*ngx_http_accounting_hash_iterate_func)(u_char *name,
                size_t len, void *val, void *para1, void *para2);

ngx_int_t ngx_http_accounting_hash_init(ngx_http_accounting_hash_t *hash,
                ngx_uint_t nr_buckets, ngx_pool_t *pool);

ngx_int_t ngx_http_accounting_hash_add(ngx_http_accounting_hash_t *hash,
                ngx_uint_t key, u_char *name, size_t len, void *value);

ngx_int_t ngx_http_accounting_hash_iterate(ngx_http_accounting_hash_t *hash,
                ngx_http_accounting_hash_iterate_func func, void *para1, void *para2);

void * ngx_http_accounting_hash_find(ngx_http_accounting_hash_t *hash,
                ngx_uint_t key, u_char *name, size_t len);

#endif /* _NGX_HTTP_ACCOUNTING_HASH_H_INCLUDED_ */
