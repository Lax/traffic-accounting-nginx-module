#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <syslog.h>

#define _INTERVAL_ 60
#define _MAX_UNIT_NUM_ 1024
#define _DEFAULT_UNIT_NUM_ 0

typedef struct {
	ngx_int_t accounting_id;
} ngx_http_accounting_conf_t;

static ngx_pool_t	*ngx_http_accounting_id_pool;
static ngx_time_t	*tp;

static u_char *ngx_http_accounting_title = (u_char *)"NgxAccounting";

ngx_int_t accounting_bytes_out[_MAX_UNIT_NUM_];
ngx_int_t accounting_requests[_MAX_UNIT_NUM_];

ngx_int_t ngx_http_accounting_timer_old = 0;
ngx_int_t ngx_http_accounting_timer_new = 0;

static ngx_int_t ngx_http_accounting_filter_init (ngx_conf_t*);
static ngx_int_t ngx_http_accounting_header_filter(ngx_http_request_t*);

static ngx_int_t ngx_http_accounting_add_variables(ngx_conf_t *cf);
static ngx_int_t ngx_http_accounting_requests_variable(ngx_http_request_t *r,    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_accounting_bytes_sent_variable(ngx_http_request_t *r,    ngx_http_variable_value_t *v, uintptr_t data);

static void * ngx_http_accounting_create_loc_conf(ngx_conf_t *cf);
static char * ngx_http_accounting_merge_loc_conf(ngx_conf_t *cf, void*, void*);

static ngx_int_t ngx_http_accounting_process_init(ngx_cycle_t *cycle);
static void ngx_http_accounting_process_exit(ngx_cycle_t *cycle);

static ngx_int_t ngx_http_accounting_log_write_out();
static ngx_int_t ngx_http_accounting_record_syslog(const char *fmt, ...);

static ngx_command_t  ngx_http_accounting_commands[] = {

	{ ngx_string("accounting_id"),				/* name */
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,                     /* type */
		ngx_conf_set_num_slot,				/* set~~~ */
		NGX_HTTP_LOC_CONF_OFFSET,			/* conf */
		offsetof(ngx_http_accounting_conf_t, accounting_id),	/* offset */
		NULL						/* post */ },

	ngx_null_command
};

static ngx_http_module_t  ngx_http_accounting_ctx = {
	ngx_http_accounting_add_variables,		/* preconfiguration */
	ngx_http_accounting_filter_init,		/* postconfiguration */
	NULL,						/* create main configuration */
	NULL,						/* init main configuration */
	NULL,						/* create server configuration */
	NULL,						/* merge server configuration */
	ngx_http_accounting_create_loc_conf,		/* create location configuration */
	ngx_http_accounting_merge_loc_conf		/* merge location configuration */
};

ngx_module_t ngx_http_accounting_module = {
	NGX_MODULE_V1,
	&ngx_http_accounting_ctx,			/* module context */
	ngx_http_accounting_commands,			/* module directives */
	NGX_HTTP_MODULE,				/* module type */
	NULL,						/* init master */
	NULL,						/* init module */
	ngx_http_accounting_process_init,		/* init process */
	NULL,						/* init thread */
	NULL,						/* exit thread */
	ngx_http_accounting_process_exit,		/* exit process */
	NULL,						/* exit master */
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

	return NGX_OK;
}

static ngx_int_t
ngx_http_accounting_get_accounting_id(ngx_http_request_t *r)
{

	static ngx_str_t *accounting_id_str = NULL;
	static ngx_uint_t key = 0;

	ngx_http_variable_value_t *vv;
	if (accounting_id_str==NULL) {
		if((accounting_id_str = ngx_palloc(ngx_http_accounting_id_pool, sizeof(*accounting_id_str) + sizeof("accounting_id")))!=NULL) {;
			accounting_id_str->len = sizeof("accounting_id") -1;
			accounting_id_str->data = (u_char *)(accounting_id_str + 1);
			ngx_memcpy(accounting_id_str->data, (u_char *)"accounting_id", accounting_id_str->len);
			key = ngx_hash_strlow(accounting_id_str->data, accounting_id_str->data, accounting_id_str->len);
		}
		else {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "unable to allocate memory for $accounting_id variable name.");
		}
	}
	
	vv = ngx_http_get_variable(r, accounting_id_str, key, 0);
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

	ngx_int_t		accounting_id = ngx_http_accounting_get_accounting_id(r);
	accounting_bytes_out[accounting_id] += r->connection->sent; // we get zero here.

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

	ngx_int_t		accounting_id = ngx_http_accounting_get_accounting_id(r);
	accounting_requests[accounting_id] ++;

	v->valid = 1;
	v->no_cacheable = 0;
	v->not_found = 0;

	len = sizeof(accounting_requests[accounting_id]);
	p = ngx_pnalloc(r->pool, len);

	v->data = p;

	p = ngx_sprintf(p, "%d", accounting_requests[accounting_id]);

	v->len = p - v->data;

	return NGX_OK;
}
// VARS end

void ngx_http_accounting_alarm_handler(int sig) {
	ngx_http_accounting_log_write_out();
}

// process init
static ngx_int_t
ngx_http_accounting_process_init(ngx_cycle_t *cycle)
{
	struct sigaction  sa;
	struct itimerval  itv;

	openlog((char *)ngx_http_accounting_title, LOG_NDELAY, LOG_SYSLOG);

	ngx_memzero(&sa, sizeof(struct sigaction));
	sa.sa_handler = ngx_http_accounting_alarm_handler;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGALRM, &sa, NULL) == -1) {
	ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
		"sigaction(SIGALRM) failed");
	return NGX_ERROR;
	}

	itv.it_interval.tv_sec = 60;
	itv.it_interval.tv_usec = 0;
	itv.it_value.tv_sec = 60;
	itv.it_value.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
		ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
			"setitimer() failed");
	}

	ngx_http_accounting_id_pool = ngx_create_pool(1024, 0);

	int i;
	for (i = 0; i < _MAX_UNIT_NUM_; i++) {
		accounting_bytes_out[i] = 0;
		accounting_requests[i] = 0;
	}

	ngx_http_accounting_record_syslog("pid:%i|Process:init", ngx_getpid());

	return NGX_OK;
}

static void
ngx_http_accounting_process_exit(ngx_cycle_t *cycle)
{
	ngx_http_accounting_log_write_out();
	ngx_http_accounting_record_syslog("pid:%i|Process:exit", ngx_getpid());
	//closelog();

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

	conf->accounting_id = NGX_CONF_UNSET;

	return conf;
};

static char *
ngx_http_accounting_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_accounting_conf_t *prev = parent;
	ngx_http_accounting_conf_t *conf = child;

	ngx_conf_merge_value(conf->accounting_id, prev->accounting_id, 0);

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
ngx_http_accounting_log_write_out()
{
	tp = ngx_timeofday();
	ngx_http_accounting_timer_old = ngx_http_accounting_timer_new; // - _INTERVAL_;
	ngx_http_accounting_timer_new = tp->sec;

	int i;
	for (i = 0; i < _MAX_UNIT_NUM_; i++) {
		if(accounting_requests[i] > 0) {
			ngx_http_accounting_record_syslog("pid:%i|from:%i|to:%i|accounting_id:%d|requests:%d|bytes_out:%d", ngx_getpid(), ngx_http_accounting_timer_old, ngx_http_accounting_timer_new, i, accounting_requests[i], accounting_bytes_out[i]);
			accounting_requests[i] = 0;
			accounting_bytes_out[i] = 0;
		}
	}

	return NGX_OK;
}

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static ngx_int_t
ngx_http_accounting_header_filter(ngx_http_request_t *r)
{
	return ngx_http_next_header_filter(r);
}

static ngx_int_t
ngx_http_accounting_filter_init (ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_accounting_header_filter;

    return NGX_OK;
}

