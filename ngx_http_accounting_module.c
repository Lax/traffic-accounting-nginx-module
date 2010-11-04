#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <syslog.h>
#include <signal.h>

#define INTERVAL	60

#define DEFAULT_CHANNEL 	0
#define MAX_CHANNEL 		1024

typedef struct {
	ngx_int_t accounting_id;
} ngx_http_accounting_conf_t;

static ngx_pool_t	*ngx_http_accounting_id_pool;
static ngx_time_t	*tp;

static u_char *ngx_http_accounting_title = (u_char *)"NgxAccounting";

ngx_int_t write_out = 0;

ngx_int_t accounting_bytes_out[MAX_CHANNEL];
ngx_int_t accounting_requests[MAX_CHANNEL];

ngx_int_t ngx_http_accounting_timer_old = 0;
ngx_int_t ngx_http_accounting_timer_new = 0;

static ngx_int_t ngx_http_accounting_init (ngx_conf_t*);

static void * ngx_http_accounting_create_loc_conf(ngx_conf_t *cf);
static char * ngx_http_accounting_merge_loc_conf(ngx_conf_t *cf, void*, void*);

static ngx_int_t ngx_http_accounting_process_init(ngx_cycle_t *cycle);
static void ngx_http_accounting_process_exit(ngx_cycle_t *cycle);

static ngx_int_t ngx_http_accounting_log_write_out();
static ngx_int_t ngx_http_accounting_record_syslog(const char *fmt, ...);

static ngx_command_t  ngx_http_accounting_commands[] = {
	{ ngx_string("accounting_id"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_num_slot,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_accounting_conf_t, accounting_id),
	  NULL},

	ngx_null_command
};

static ngx_http_module_t  ngx_http_accounting_ctx = {
	NULL,									/* preconfiguration */
	ngx_http_accounting_init,		        /* postconfiguration */
	NULL,									/* create main configuration */
	NULL,									/* init main configuration */
	NULL,									/* create server configuration */
	NULL,									/* merge server configuration */
	ngx_http_accounting_create_loc_conf,	/* create location configuration */
	ngx_http_accounting_merge_loc_conf		/* merge location configuration */
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


static ngx_int_t
ngx_http_accounting_get_accounting_id(ngx_http_request_t *r)
{
	static ngx_str_t *accounting_id_str = NULL;
	static ngx_uint_t key = 0;

	ngx_http_variable_value_t *vv;
	
	if (accounting_id_str == NULL) {
		accounting_id_str = ngx_palloc(ngx_http_accounting_id_pool,
								sizeof(*accounting_id_str) + sizeof("accounting_id"));
		if (accounting_id_str != NULL) {
			accounting_id_str->len = sizeof("accounting_id") -1;
			accounting_id_str->data = (u_char *)(accounting_id_str + 1);
			ngx_memcpy(accounting_id_str->data, (u_char *)"accounting_id", accounting_id_str->len);
			key = ngx_hash_strlow(accounting_id_str->data, accounting_id_str->data, accounting_id_str->len);
		}
		else
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"unable to allocate memory for $accounting_id variable name.");
	}

	vv = ngx_http_get_variable(r, accounting_id_str, key);
	if (vv!=NULL && !vv->not_found && vv->len!=0) {
		ngx_int_t id = ngx_atoi(vv->data, vv->len);
		if( id> 0 && id < MAX_CHANNEL)
			return id;
	}

	return DEFAULT_CHANNEL;
}


void ngx_http_accounting_alarm_handler(int sig) {
	write_out = 1;
}


static ngx_int_t
ngx_http_accounting_process_init(ngx_cycle_t *cycle)
{
	openlog((char *)ngx_http_accounting_title, LOG_NDELAY, LOG_SYSLOG);

	struct sigaction  sa;

	ngx_memzero(&sa, sizeof(struct sigaction));
	sa.sa_handler = ngx_http_accounting_alarm_handler;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGALRM, &sa, NULL) == -1) {
		ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, "sigaction failed");
		return NGX_ERROR;
	}

	struct itimerval  itv;
	itv.it_interval.tv_sec = INTERVAL;
	itv.it_interval.tv_usec = 0;
	itv.it_value.tv_sec = INTERVAL;
	itv.it_value.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
		ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, "setitimer failed");
		return NGX_ERROR;
	}

	ngx_http_accounting_id_pool = ngx_create_pool(1024, cycle->log);

	int i;
	for (i = 0; i < MAX_CHANNEL; i++) {
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

	// optional
	// closelog();
}


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

	ngx_http_accounting_timer_old = ngx_http_accounting_timer_new;
	ngx_http_accounting_timer_new = tp->sec;

	int i;
	for (i = 0; i < MAX_CHANNEL; i++) {
		if(accounting_requests[i] > 0) {
			ngx_http_accounting_record_syslog("pid:%i|from:%i|to:%i|accounting_id:%d|requests:%d|bytes_out:%d",
					ngx_getpid(),
					ngx_http_accounting_timer_old, ngx_http_accounting_timer_new,
					i, accounting_requests[i], accounting_bytes_out[i]);

			accounting_requests[i] = 0;
			accounting_bytes_out[i] = 0;
		}
	}

	return NGX_OK;
}


ngx_int_t
ngx_http_accounting_handler(ngx_http_request_t *r)
{
	ngx_int_t		accounting_id;

	accounting_id = ngx_http_accounting_get_accounting_id(r);

	accounting_requests[accounting_id]  ++;
	accounting_bytes_out[accounting_id] += r->connection->sent;

	if (write_out) {
		ngx_http_accounting_log_write_out();
		write_out = 0;
	}

	return NGX_OK;
}


static ngx_int_t
ngx_http_accounting_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_accounting_handler;

    return NGX_OK;
}
