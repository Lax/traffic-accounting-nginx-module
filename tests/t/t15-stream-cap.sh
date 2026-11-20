#!/usr/bin/env bash
# t15: stream subsystem — cap + overflow with per-connection unique ids
source "${TESTS_DIR}/lib/harness.sh"
require_variant stream
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
cat > "$CONF" <<EOF
worker_processes 1;
error_log $RUNDIR/error.log warn;
pid $RUNDIR/nginx.pid;
events { worker_connections 8192; }
stream {
    accounting on;
    accounting_log $RUNDIR/accounting.log;
    accounting_interval 1;
    accounting_max_metrics 16;
    server {
        listen 127.0.0.1:$PORT;
        accounting_id \$remote_port;
        return "ok";
    }
}
EOF

start_nginx "$CONF"
wait_master || { fail "nginx did not come up"; finish; }

# 30 connections, each with a distinct remote port -> 30 distinct ids, cap 16
for i in $(seq 1 30); do tcp_once "$PORT"; done

wait_acct 10 '[[ $(sum_field __overflow__ sessions 2>/dev/null || echo 0) -ge 14 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "16 organic port ids exported" 16 "$(acct ids | grep -cE '^[0-9]{4,5}$')"
check_eq "overflow holds 14 excess sessions" 14 "$(sum_field __overflow__ sessions)"
check_eq "overflow alert fired once" 1 "$(grep -c 'more than 16 accounting ids' "$ACCT_LOG")"
finish
