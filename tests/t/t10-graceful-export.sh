#!/usr/bin/env bash
# t10: graceful shutdown exports the current (unrotated) period
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 60; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

# interval is 60s: no rotation will fire during this test
for i in $(seq 1 7); do http_get "$PORT" "/?uid=exit-export" >/dev/null; done
sleep 0.3

stop_nginx_graceful

check_eq "final period exported at graceful exit" 7 "$(sum_field exit-export requests)"
finish
