#!/usr/bin/env bash
# t02: variable accounting_id ($arg_uid) — separate ids counted separately
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

for i in $(seq 1 5);  do http_get "$PORT" "/?uid=alpha" >/dev/null; done
for i in $(seq 1 3);  do http_get "$PORT" "/?uid=beta"  >/dev/null; done

wait_acct 8 '[[ $(sum_field alpha requests) -ge 5 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "alpha==5" 5 "$(sum_field alpha requests)"
check_eq "beta==3" 3 "$(sum_field beta requests)"
check_eq "two ids exported" 2 "$(acct ids | wc -l)"
finish
