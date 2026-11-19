#!/usr/bin/env bash
# t14: stream subsystem — basic session accounting
source "${TESTS_DIR}/lib/harness.sh"
require_variant stream
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
stream_conf "$PORT" 1 > "$CONF"

start_nginx "$CONF"
wait_master || { fail "nginx did not come up"; finish; }

for i in $(seq 1 6); do tcp_once "$PORT"; done

wait_acct 8 '[[ $(acct sum --field sessions 2>/dev/null || echo 0) -ge 6 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

# stream accounting_id defaults to __default__ when not set
check_eq "6 sessions counted" 6 "$(sum_field __default__ sessions 2>/dev/null || echo 999)"
# the server sends "ok" per session; the return-server never reads input,
# so bytes_in is legitimately 0 — assert on bytes_out instead
check_ge "bytes_out covers 6x2-byte replies" 12 "$(sum_field __default__ bytes_out 2>/dev/null || echo 0)"
check_eq "stream status 200 counted" 6 "$(acct status --id __default__ --code 200)"
finish
