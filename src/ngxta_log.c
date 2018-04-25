
/*
 * Copyright (C) Liu Lantao
 */


#include "ngxta.h"

ngx_log_t ngxta_log;

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
