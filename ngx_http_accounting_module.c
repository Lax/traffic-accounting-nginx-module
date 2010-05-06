#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <syslog.h>

#define _INTERVAL_ 60
#define _MAX_UNIT_NUM_ 1024
#define _DEFAULT_UNIT_NUM_ 0

typedef struct {
	ngx_int_t accounting_unit;
} ngx_http_accounting_conf_t;

static ngx_pool_t	*ngx_http_accounting_unit_id_pool;

static u_char *ngx_http_accounting_title = (u_char *)"NgxAccounting";

ngx_int_t accounting_bytes_out[_MAX_UNIT_NUM_];
ngx_int_t accounting_requests[_MAX_UNIT_NUM_];

ngx_int_t ngx_http_accounting_timer_old = 0;
ngx_int_t ngx_http_accounting_timer_new = 0;
ngx_uint_t ngx_http_accounting_requests = 0;
ngx_uint_t ngx_http_accounting_bytes_sent = 0;

static ngx_int_t ngx_http_accounting_unit_filter_init (ngx_conf_t*);
static ngx_int_t ngx_http_accounting_unit_header_filter(ngx_http_request_t*);

static ngx_int_t ngx_http_accounting_add_variables(ngx_conf_t *cf);
static ngx_int_t ngx_http_accounting_requests_variable(ngx_http_request_t *r,    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_accounting_bytes_sent_variable(ngx_http_request_t *r,    ngx_http_variable_value_t *v, uintptr_t data);

static void * ngx_http_accounting_create_loc_conf(ngx_conf_t *cf);
static char * ngx_http_accounting_merge_loc_conf(ngx_conf_t *cf, void*, void*);

static ngx_int_t ngx_http_accounting_process_init(ngx_cycle_t *cycle);
static void ngx_http_accounting_process_exit(ngx_cycle_t *cycle);

static ngx_int_t ngx_http_accounting_conditional_log();
static ngx_int_t ngx_http_accounting_record_syslog(const char *fmt, ...);

static ngx_command_t  ngx_http_accounting_commands[] = {

	{ ngx_string("accounting_unit"),                   /* name */
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,                     /* type */
		ngx_conf_set_num_slot,				/* set~~~ */
		NGX_HTTP_LOC_CONF_OFFSET,			/* conf */
		offsetof(ngx_http_accounting_conf_t, accounting_unit),	/* offset */
		NULL                        /* post */ },

	ngx_null_command
};

static ngx_http_module_t  ngx_http_accounting_ctx = {
	ngx_http_accounting_add_variables,          /* preconfiguration */
	ngx_http_accounting_unit_filter_init,                   /* postconfiguration */
	NULL,                 /* create main configuration */
	NULL,                 /* init main configuration */
	NULL,                 /* create server configuration */
	NULL,                 /* merge server configuration */
	ngx_http_accounting_create_loc_conf,			/* create location configuration */
	ngx_http_accounting_merge_loc_conf			/* merge location configuration */
};

ngx_module_t ngx_http_accounting_module = {
	NGX_MODULE_V1,
	&ngx_http_accounting_ctx,				/* module context */
	ngx_http_accounting_commands,			/* module directives */
	NGX_HTTP_MODULE,						/* module type */
	NULL,									/* init master */
	NULL,									/* init module */
	ngx_http_accounting_process_init,		/* init process */
	NULL,									/* init thread */
	NULL,									/* exit thread */
	ngx_http_accounting_process_exit,		/* exit process */
	NULL,									/* exit master */
	NGX_MODULE_V1_PADDING
};

// VARS defination
static ngx_http_variable_t ngx_http_accounting_vars[] = {
	{ ngx_string("accounting_requests"), NULL,
	ngx_http_accounting_requests_variable, 0,
	NGX_HTTP_VAR_NOHASH|NGX_HTTP_VAR_NOCACHEABLE, 0 },

	{ ngx_string("accounting_bytes_sent"), NULL,
	ngx_http_accounting_bytes_sent_variable, 0,
	NGX_HTTP_VAR_NOHASH|NGX_HTTP_VAR_NOCACHEABLE, 0 },

	{ ngx_null_string, NULL, NULL, 0, 0, 0 }
};

static ngx_int_t
ngx_http_accounting_add_variables(ngx_conf_t *cf)
{
	ngx_http_variable_t *var, *v;

	for (v = ngx_http_accounting_vars; v->name.len; v++) {
		var = ngx_http_add_variable(cf, &v->name, v->flags);
		if (var == NULL) {
			return NGX_ERROR;
		}

	var->get_handler = v->get_handler;
	var->data = v->data;
	}

	ngx_http_accounting_conditional_log();

	return NGX_OK;
}

static ngx_int_t
ngx_http_accounting_get_unit_id(ngx_http_request_t *r)
{

	static ngx_str_t *unit_id_str = NULL;
	static ngx_uint_t key = 0;

	ngx_http_variable_value_t *vv;
	if (unit_id_str==NULL) {
		if((unit_id_str = ngx_palloc(ngx_http_accounting_unit_id_pool, sizeof(*unit_id_str) + sizeof("unit_id")))!=NULL) {;
			unit_id_str->len = sizeof("unit_id") -1;
			unit_id_str->data = (u_char *)(unit_id_str + 1);
			ngx_memcpy(unit_id_str->data, (u_char *)"unit_id", unit_id_str->len);
			key = ngx_hash_strlow(unit_id_str->data, unit_id_str->data, unit_id_str->len);
		}
		else {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "unable to allocate memory for $unit_id variable name.");
		}
	}
	
	vv = ngx_http_get_variable(r, unit_id_str, key);
	if(vv!=NULL && !vv->not_found && vv->len!=0) {
		if(ngx_atoi(vv->data, vv->len) > 0 && ngx_atoi(vv->data, vv->len) < _MAX_UNIT_NUM_) {
			return ngx_atoi(vv->data, vv->len);
		} else {
			return _DEFAULT_UNIT_NUM_;
		}
	}
	
	return _DEFAULT_UNIT_NUM_;
}

static ngx_int_t
ngx_http_accounting_bytes_sent_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
	off_t			sent;
	u_char			*p;

	ngx_int_t		accounting_unit = ngx_http_accounting_get_unit_id(r);
	accounting_bytes_out[accounting_unit] += r->connection->sent; // we get zero here.

	sent = r->connection->sent;
		
	p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
	if (p == NULL) {
		return NGX_ERROR;
	}

	v->len = ngx_sprintf(p, "%O", r->connection->sent) - p;
	v->valid = 1;
	v->no_cacheable = 0;
	v->not_found = 0;
	v->data = p;

	return NGX_OK;
}


static ngx_int_t
ngx_http_accounting_requests_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
	u_char			*p;
	size_t			len;

	ngx_int_t		accounting_unit = ngx_http_accounting_get_unit_id(r);
	accounting_requests[accounting_unit] ++;

	v->valid = 1;
	v->no_cacheable = 0;
	v->not_found = 0;

	len = sizeof(accounting_requests[accounting_unit]);
	p = ngx_pnalloc(r->pool, len);

	v->data = p;

	p = ngx_sprintf(p, "%d", accounting_requests[accounting_unit]);

	v->len = p - v->data;

	return NGX_OK;
}
// VARS end

// process init
static ngx_int_t
ngx_http_accounting_process_init(ngx_cycle_t *cycle)
{
	ngx_http_accounting_unit_id_pool = ngx_create_pool(1024, 0);

	openlog((char *)ngx_http_accounting_title, LOG_ODELAY, LOG_SYSLOG);

	int i;
	for (i = 0; i < _MAX_UNIT_NUM_; i++) {
		accounting_bytes_out[i] = 0;
		accounting_requests[i] = 0;
	}

	ngx_http_accounting_record_syslog("pid:%i|Process init", ngx_getpid());

	return NGX_OK;
}

static void
ngx_http_accounting_process_exit(ngx_cycle_t *cycle)
{
	ngx_http_accounting_record_syslog("pid:%i|Process exit", ngx_getpid());

	closelog();

	return;
}
// process end

static void *
ngx_http_accounting_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_accounting_conf_t *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_accounting_conf_t));
	if(conf == NULL) {
		return NGX_CONF_ERROR;
	}

	conf->accounting_unit = NGX_CONF_UNSET;

	return conf;
};

static char *
ngx_http_accounting_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_accounting_conf_t *prev = parent;
	ngx_http_accounting_conf_t *conf = child;

	ngx_conf_merge_value(conf->accounting_unit, prev->accounting_unit, 0);

	return NGX_CONF_OK;
};

static ngx_int_t
ngx_http_accounting_record_syslog(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vsyslog(LOG_INFO, fmt, args);
  va_end(args);

  return NGX_OK;
};

static ngx_int_t
ngx_http_accounting_conditional_log()
{
	ngx_time_t      *tp;
	tp = ngx_timeofday();
	
	if(tp->sec >= ngx_http_accounting_timer_new)
	{
		int i = 0;
		for (i = 0; i < _MAX_UNIT_NUM_; i++) {
			if(accounting_requests[i] > 0) {
				ngx_http_accounting_record_syslog("pid:%i|from:%i|to:%i|accounting_unit_id:%d|requests:%d|bytes_out:%d", ngx_getpid(), ngx_http_accounting_timer_old, ngx_http_accounting_timer_new, i, accounting_requests[i], accounting_bytes_out[i]);
				accounting_requests[i] = 0;
				accounting_bytes_out[i] = 0;
			}
		}

		ngx_http_accounting_timer_old = tp->sec;
		ngx_http_accounting_timer_new = tp->sec / _INTERVAL_ * _INTERVAL_ + _INTERVAL_;
	}

	return NGX_OK;
}

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static ngx_int_t
ngx_http_accounting_unit_header_filter(ngx_http_request_t *r)
{
	ngx_http_accounting_conditional_log();

	return ngx_http_next_header_filter(r);
}

static ngx_int_t
ngx_http_accounting_unit_filter_init (ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_accounting_unit_header_filter;

    return NGX_OK;
}

