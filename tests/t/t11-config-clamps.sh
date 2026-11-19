#!/usr/bin/env bash
# t11: config clamps and validation warnings (nginx -t output)
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir

mkconf() { # $1=out-file $2 interval [$3 max_metrics]
    {
        cat <<EOF
worker_processes 1;
error_log stderr notice;
pid $RUNDIR/nginx.pid;
events { worker_connections 64; }
http {
    accounting on;
    accounting_log $RUNDIR/accounting.log;
    accounting_interval $2;
EOF
        [[ -n "${3:-}" ]] && echo "    accounting_max_metrics $3;"
        echo "}"
    } > "$1"
}

mkconf "$RUNDIR/c1.conf" 0
T=$("$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/c1.conf" -t 2>&1); rc=$?
check_eq "interval 0 accepted (clamped)" 0 "$rc"
check_contains "interval clamp warns" /dev/stdin <<<"$T" "accounting_interval must be at least 1s"

mkconf "$RUNDIR/c2.conf" 5 1
T=$("$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/c2.conf" -t 2>&1); rc=$?
check_eq "max_metrics 1 accepted (clamped)" 0 "$rc"
check_contains "max_metrics lower clamp warns" /dev/stdin <<<"$T" "accounting_max_metrics must be at least 16"

mkconf "$RUNDIR/c3.conf" 5 999999999
T=$("$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/c3.conf" -t 2>&1); rc=$?
check_eq "max_metrics huge accepted (clamped)" 0 "$rc"
check_contains "max_metrics upper clamp warns" /dev/stdin <<<"$T" "accounting_max_metrics clamped to 1048576"

mkconf "$RUNDIR/c4.conf" 999999999999
T=$("$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/c4.conf" -t 2>&1); rc=$?
check_eq "interval huge accepted (clamped)" 0 "$rc"
check_contains "interval upper clamp warns" /dev/stdin <<<"$T" "accounting_interval clamped to 2592000"
finish
