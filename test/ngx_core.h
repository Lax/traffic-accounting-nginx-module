#ifndef _NGX_CORE_H_INCLUDED_
#define _NGX_CORE_H_INCLUDED_

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#define ngx_inline inline


/* Basic types */
typedef unsigned char          u_char;
typedef unsigned long          ngx_uint_t;
typedef long                   ngx_int_t;
typedef int                    ngx_flag_t;
typedef time_t                 ngx_msec_t;
typedef int                    ngx_msec_int_t;
typedef unsigned long          ngx_rbtree_key_t;

/* String */
typedef struct {
    size_t  len;
    u_char *data;
} ngx_str_t;

/* Time */
typedef struct {
    time_t      sec;
    ngx_uint_t  msec;
    ngx_int_t   gmtoff;
} ngx_time_t;

/* Log */
typedef struct ngx_log_s  ngx_log_t;
struct ngx_log_s {
    unsigned    log_level;
    ngx_log_t  *next;
    void       *data;
    char       *action;
};

/* Rbtree node */
typedef struct ngx_rbtree_node_s  ngx_rbtree_node_t;
struct ngx_rbtree_node_s {
    ngx_rbtree_key_t       key;
    ngx_rbtree_node_t     *left;
    ngx_rbtree_node_t     *right;
    ngx_rbtree_node_t     *parent;
    u_char                 color;
    u_char                 data;
};

/* Rbtree insert function pointer */
typedef void (*ngx_rbtree_insert_pt)(ngx_rbtree_node_t *root,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);

/* Rbtree */
typedef struct ngx_rbtree_s  ngx_rbtree_t;
struct ngx_rbtree_s {
    ngx_rbtree_node_t    *root;
    ngx_rbtree_node_t    *sentinel;
    ngx_rbtree_insert_pt  insert;
};

/* Variable value */
typedef struct {
    unsigned    len:28;
    unsigned    valid:1;
    unsigned    no_cacheable:1;
    unsigned    not_found:1;
    unsigned    escape:1;
    u_char     *data;
} ngx_variable_value_t;


/* Return codes */
#ifndef NGX_OK
#define NGX_OK      0
#endif
#ifndef NGX_ERROR
#define NGX_ERROR   -1
#endif
#ifndef NGX_DONE
#define NGX_DONE    2
#endif

/* Log levels */
#define NGX_LOG_NOTICE      5
#define NGX_LOG_INFO        6

/* Buffer constants */
#define NGX_MAX_ERROR_STR   4096
#define NGX_LINEFEED_SIZE   2

/* Conf constants */
#define NGX_CONF_UNSET      -1
#define NGX_CONF_OK         ((char *)0)
#define NGX_CONF_ERROR      ((char *)1)


/* Memory operations */
void *ngx_calloc(size_t size, ngx_log_t *log);
void  ngx_free(void *p);

#define ngx_memcpy(dst, src, n)   memcpy(dst, src, n)
#define ngx_memcmp(s1, s2, n)     memcmp(s1, s2, n)
#define ngx_memzero(buf, n)       memset(buf, 0, n)
#define ngx_rstrncmp(s1, s2, n)   strncmp((char *)s1, (char *)s2, n)


/* Rbtree macros */
#define ngx_rbt_red(node)             ((node)->color = 1)
#define ngx_rbt_black(node)           ((node)->color = 0)
#define ngx_rbtree_sentinel_init(node)  ngx_rbt_black(node)

#define ngx_rbtree_init(tree, s, i)                                       \
    ngx_rbtree_sentinel_init(s);                                          \
    (tree)->root = s;                                                     \
    (tree)->sentinel = s;                                                 \
    (tree)->insert = i


/* Rbtree functions (implemented in test) */
void ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);
void ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);


/* Hash function */
ngx_rbtree_key_t ngx_hash_key_lc(u_char *data, size_t len);


/* Utility inlines */
static ngx_inline ngx_time_t *ngx_timeofday(void) {
    static ngx_time_t t;
    t.sec = time(NULL);
    return &t;
}

static ngx_inline ngx_int_t ngx_getpid(void) { return 0; }


#endif /* _NGX_CORE_H_INCLUDED_ */
