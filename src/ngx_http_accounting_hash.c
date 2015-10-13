#include <ngx_config.h>
#include <ngx_core.h>

#include "ngx_http_accounting_hash.h"


typedef struct {
    void             *value;
    u_char            len;
    u_char           *name;
} ngx_http_accounting_hash_elt_t;


ngx_int_t
ngx_http_accounting_hash_init(ngx_http_accounting_hash_t *hash,
        ngx_uint_t nr_buckets, ngx_pool_t *pool)
{
    hash->size = nr_buckets;
    hash->pool = pool;

    hash->buckets = ngx_pcalloc(pool, sizeof(ngx_array_t *) * hash->size);
    if (hash->buckets == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

ngx_int_t
ngx_http_accounting_hash_add(ngx_http_accounting_hash_t *hash,
        ngx_uint_t key, u_char *name, size_t len, void *value)
{
    ngx_array_t     *bucket;
    ngx_http_accounting_hash_elt_t *elt;
    void *data;

    bucket = hash->buckets[key % hash->size];

    if (bucket == NULL) {
        bucket = ngx_pcalloc(hash->pool, sizeof(ngx_array_t));

        if (bucket == NULL) {
            return NGX_ERROR;
        }

        if (ngx_array_init(bucket, hash->pool, 3, sizeof(ngx_http_accounting_hash_elt_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        hash->buckets[key % hash->size] = bucket;
    }

    elt = ngx_array_push(bucket);
    if (elt == NULL)
        return NGX_ERROR;

    data = ngx_pcalloc(hash->pool, len+1);
    ngx_memcpy(data, name, len);
    elt->name = data;
    elt->len = len;
    elt->value = value;

    return NGX_OK;
}

void *
ngx_http_accounting_hash_find(ngx_http_accounting_hash_t *hash,
        ngx_uint_t key, u_char *name, size_t len)
{
    ngx_uint_t       i, j;
    ngx_array_t     *bucket;
    ngx_http_accounting_hash_elt_t *elt;
    ngx_http_accounting_hash_elt_t *elts;

    bucket = hash->buckets[key % hash->size];

    if (bucket == NULL) {
        return NULL;
    }

    for (i=0; i<bucket->nelts; i++) {
        elts = (ngx_http_accounting_hash_elt_t *)bucket->elts;
        elt = &elts[i];

        if (len != (size_t) elt->len) {
            continue;
        }

        for (j = 0; j<len; j++) {
            if (name[j] != elt->name[j]) {
                break;
            }
        }

        if (j == len) {
            return elt->value;
        }
    }

    return NULL;
}

ngx_int_t
ngx_http_accounting_hash_iterate(ngx_http_accounting_hash_t *hash,
        ngx_http_accounting_hash_iterate_func func, void *para1, void *para2)
{
    ngx_uint_t       i;
    ngx_uint_t       j;

    ngx_int_t        ret_code;
    ngx_array_t     *bucket;
    ngx_http_accounting_hash_elt_t  *elt;
    ngx_http_accounting_hash_elt_t  *elts;

    for (i=0; i < hash->size; i++) {
        bucket = hash->buckets[i];

        if (bucket == NULL)
            continue;

        for (j=0; j<bucket->nelts; j++) {
            elts = (ngx_http_accounting_hash_elt_t  *)bucket->elts;
            elt = &elts[j];

            if (elt->value && func) {
                ret_code = func(elt->name, elt->len, elt->value, para1, para2);
            } else {
                continue;
            }

            if (ret_code != NGX_OK)
                return ret_code;
        }
    }

    return NGX_OK;
}
