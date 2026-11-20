#!/usr/bin/env bash
# t12: multiple workers — per-worker periods, totals conserved
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
WORKERS=2
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

for i in $(seq 1 20); do http_get "$PORT" "/?uid=multi" >/dev/null; done

wait_acct 10 '[[ $(sum_field multi requests 2>/dev/null || echo 0) -ge 20 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "20 requests conserved across workers" 20 "$(sum_field multi requests)"
check_ge "at least one worker pid exported" 1 "$(acct lines | python3 -c 'import json,sys; print(len({l["pid"] for l in json.load(sys.stdin)}))')"
finish
