#!/usr/bin/env bash
# t09: empty periods (no traffic) rotate cleanly; later traffic still counted
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

# let 3+ empty periods rotate by
sleep 3.5

check_eq "no export for empty periods" 0 "$(grep -c 'accounting_id:' "$ACCT_LOG" || true)"
check_eq "nginx still alive after idle rotations" 200 "$(http_get "$PORT" "/?uid=ping" 2>/dev/null || echo 000)"

for i in $(seq 1 5); do http_get "$PORT" "/?uid=after-idle" >/dev/null; done
wait_acct 8 '[[ $(sum_field after-idle requests 2>/dev/null || echo 0) -ge 5 ]]' \
    || { fail "no export after idle"; finish; }
stop_nginx_graceful

check_eq "traffic after idle periods counted" 5 "$(sum_field after-idle requests)"
finish
