#include "ngxta.h"

static ngx_str_t err_levels[] = {
    ngx_null_string,
    ngx_string("emerg"),
    ngx_string("alert"),
    ngx_string("crit"),
    ngx_string("error"),
    ngx_string("warn"),
    ngx_string("notice"),
    ngx_string("info"),
    ngx_string("debug")
};

ngx_log_t *ngxta_logger = NULL;

ngx_int_t
ngxta_log_open(ngx_cycle_t *cycle, ngx_log_t *log, ngx_str_t *log_path)
{
    log->log_level = NGX_LOG_NOTICE;

    log->file = ngx_conf_open_file(cycle, log_path);
    if (log->file == NULL) {
        return NGX_ERROR;
    }
    log->file->fd = ngx_open_file(&log->file->name, NGX_FILE_APPEND,
                                NGX_FILE_CREATE_OR_OPEN,
                                NGX_FILE_DEFAULT_ACCESS);

    if (log->file->fd == NGX_INVALID_FILE) {
        ngx_log_stderr(ngx_errno,
                       "[alert] could not open traffic accounting log file: "
                       ngx_open_file_n " \"%s\" failed", log->file->name.data);
    }

    return NGX_OK;
}

void
#if (NGX_HAVE_VARIADIC_MACROS)
ngxta_log(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)
#else
ngxta_log(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list      args;
#endif
    u_char       errstr[NGX_MAX_ERROR_STR];
    ssize_t      n;
    u_char       *p, *last;

    if (log == NULL)
        return;

    last = errstr + NGX_MAX_ERROR_STR;

    p = ngx_cpymem(errstr, ngx_cached_err_log_time.data,
                   ngx_cached_err_log_time.len);

    p = ngx_slprintf(p, last, " [%V] ", &err_levels[level]);

    /* pid#tid */
    p = ngx_slprintf(p, last, "%P#" NGX_TID_T_FMT ": ",
                    ngx_log_pid, ngx_log_tid);

    if (log->connection) {
        p = ngx_slprintf(p, last, "*%uA ", log->connection);
    }

#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);
#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif

    if (err) {
        p = ngx_log_errno(p, last, err);
    }

    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }

    ngx_linefeed(p);

    n = ngx_write_fd(log->file->fd, errstr, p - errstr);

    if (n == -1 && ngx_errno == NGX_ENOSPC) {
        log->disk_full_time = ngx_time();
    }

    // if (log->file->fd == ngx_stderr) {
    //     // wrote_stderr = 1;
    // }
}
