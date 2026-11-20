#!/usr/bin/env bash
# t13: SIGHUP reload mid-period — no crash, totals conserved across the
# retiring worker's final export and the new worker's period
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 60; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

for i in $(seq 1 6); do http_get "$PORT" "/?uid=reload" >/dev/null; done
sleep 0.2

"$NGINX_BIN" -p "$RUNDIR/" -c "$CONF" -s reload
sleep 0.8   # let the old worker retire and export its partial period
wait_port "$PORT"

for i in $(seq 1 6); do http_get "$PORT" "/?uid=reload" >/dev/null; done

stop_nginx_graceful

check_eq "12 requests conserved across reload" 12 "$(sum_field reload requests)"
finish
