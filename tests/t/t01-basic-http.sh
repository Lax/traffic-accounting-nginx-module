#!/usr/bin/env bash
# t01: fixed accounting_id — basic counting, statuses, bytes
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

for i in $(seq 1 12); do http_get "$PORT" /fixed >/dev/null; done

wait_acct 8 '[[ $(sum_field fixed_id requests) -ge 12 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "requests==12" 12 "$(sum_field fixed_id requests)"
check_eq "status 200==12" 12 "$(acct status --id fixed_id --code 200)"
check_ge "bytes_in>0" 1 "$(sum_field fixed_id bytes_in)"
check_ge "bytes_out>0" 1 "$(sum_field fixed_id bytes_out)"
check_eq "exactly one exported id" 1 "$(acct ids | wc -l)"
finish
