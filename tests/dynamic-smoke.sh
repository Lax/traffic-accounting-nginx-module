#!/usr/bin/env bash
#
# Dynamic-module smoke test.
#
# A --add-dynamic-module build of this module produces a single .so that
# registers BOTH the HTTP and STREAM accounting modules (see README,
# "Installation": "both HTTP and STREAM module, target module file name is
# ngx_http_accounting_module.so").  This smoke verifies exactly that
# contract against the built binary, which the static functional suite
# cannot:
#
#   1. nginx -t accepts a config with load_module;
#   2. both subsystems account traffic after the module is loaded, with
#      the expected per-request counters;
#   3. a config reload (SIGHUP) re-registers the module and accounting
#      keeps working in the new workers.
#
# Scope: presence-level (loads, registers, accounts, survives reload).
# Exact counting/conservation across versions and rotations is the static
# functional suite's job (tests/run-tests.sh).
#
# Usage: dynamic-smoke.sh <build-out-dir>   (as produced by build-nginx.sh --dynamic)

set -u

OUT_DIR=${1:?usage: dynamic-smoke.sh <build-out-dir>}
OUT_DIR=$(cd "$OUT_DIR" && pwd)
NGINX_BIN="$OUT_DIR/nginx"

[[ -x "$NGINX_BIN" ]] || { echo "not executable: $NGINX_BIN" >&2; exit 2; }
SOS=$(ls "$OUT_DIR"/*.so 2>/dev/null | sort)
[[ -n "$SOS" ]] || { echo "no .so files in $OUT_DIR -- build with --dynamic" >&2; exit 2; }

FAILS=0
ok()   { echo "ok - $*"; }
fail() { echo "NOT OK - $*" >&2; FAILS=$((FAILS+1)); }

RUNDIR=$(mktemp -d /tmp/ngx-ta-dyn.XXXXXX)
ACCT_LOG="$RUNDIR/accounting.log"

cleanup() {
    local pid
    pid=$(cat "$RUNDIR/nginx.pid" 2>/dev/null || true)
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        kill -TERM "$pid" 2>/dev/null
        sleep 0.3
        kill -0 "$pid" 2>/dev/null && kill -9 "$pid" 2>/dev/null
    fi
    return 0
}
trap 'cleanup; rm -rf "$RUNDIR"' EXIT

pick_port() {
    python3 - <<'EOF'
import socket
s = socket.socket()
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
EOF
}
HTTP_PORT=$(pick_port)
STREAM_PORT=$(pick_port)

CONF="$RUNDIR/nginx.conf"
{
    for so in $SOS; do
        echo "load_module $so;"
    done
    cat <<EOF
worker_processes 1;
error_log $RUNDIR/error.log info;
pid $RUNDIR/nginx.pid;
events { worker_connections 1024; }
http {
    access_log off;
    accounting on;
    accounting_log $ACCT_LOG;
    accounting_interval 1;
    server {
        listen 127.0.0.1:$HTTP_PORT;
        location / { accounting_id http_dyn; return 200 "ok"; }
    }
}
stream {
    accounting on;
    accounting_log $ACCT_LOG;
    accounting_interval 1;
    server {
        listen 127.0.0.1:$STREAM_PORT;
        return "ok";
    }
}
EOF
} > "$CONF"

# 1. config check: the .so must load and register both module types
if "$NGINX_BIN" -p "$RUNDIR/" -c "$CONF" -t >/dev/null 2>&1; then
    ok "config with load_module accepted by nginx -t"
else
    fail "nginx -t rejected the dynamic module config"
    exit 1
fi

"$NGINX_BIN" -p "$RUNDIR/" -c "$CONF" >/dev/null 2>&1 \
    || { fail "nginx failed to start with dynamic module"; exit 1; }

wait_acct() { # $1 timeout-s, $2 grep condition on $ACCT_LOG
    local tries=0
    while :; do
        grep -q "$2" "$ACCT_LOG" 2>/dev/null && return 0
        sleep 0.25; tries=$((tries+1))
        (( tries * 25 >= $1 * 100 )) && return 1
    done
}

# 2. both subsystems account through the loaded module
curl -s -o /dev/null "http://127.0.0.1:$HTTP_PORT/" 2>/dev/null
( exec 3<>"/dev/tcp/127.0.0.1/$STREAM_PORT" && timeout 2 head -c 4 <&3 >/dev/null ) 2>/dev/null

if wait_acct 8 "accounting_id:http_dyn"; then
    ok "http accounted via dynamic module"
else
    fail "http traffic not accounted via dynamic module"
fi
if wait_acct 8 "sessions:"; then
    ok "stream accounted via dynamic module"
else
    fail "stream traffic not accounted via dynamic module"
fi
# one request and one session were sent; each must have been counted once
# (catches double-counting that a load-only smoke would miss)
if grep -qF 'accounting_id:http_dyn|requests:1' "$ACCT_LOG"; then
    ok "http request counted exactly once"
else
    fail "http counter is not requests:1 (double-count or loss)"
fi
if grep -qF 'accounting_id:__default__|sessions:1' "$ACCT_LOG"; then
    ok "stream session counted exactly once"
else
    fail "stream counter is not sessions:1 (double-count or loss)"
fi

# 3. reload: SIGHUP re-reads config, re-loads .so, and the NEW workers must
# account.  Assert on a worker pid that did not exist before the reload, so
# the retiring workers' graceful-exit export cannot satisfy it.  PRE_PIDS is
# newline-separated; the awk guard pads with newlines for the same reason.
# The curl is retried inside the wait: a single shot can land on a retiring
# worker (listen sockets close within ms, but why gamble), which would leave
# the new worker with no traffic to export and fail the check spuriously.
PRE_PIDS=$(grep -o 'pid:[0-9]*' "$ACCT_LOG" 2>/dev/null | sort -u || true)
"$NGINX_BIN" -p "$RUNDIR/" -c "$CONF" -s reload >/dev/null 2>&1 \
    || { fail "SIGHUP reload failed"; exit 1; }
sleep 1

new_worker_accounted() {
    local tries=0
    while :; do
        curl -s -o /dev/null --max-time 2 "http://127.0.0.1:$HTTP_PORT/" 2>/dev/null || true
        if awk -v pre="$PRE_PIDS" '
            /accounting_id:http_dyn/ {
                match($0, /pid:[0-9]+/);
                pid = substr($0, RSTART, RLENGTH);
                if (index("\n" pre "\n", "\n" pid "\n") == 0) { found = 1; exit }
            }
            END { exit !found }
        ' "$ACCT_LOG" 2>/dev/null; then
            return 0
        fi
        sleep 0.5; tries=$((tries+1))
        (( tries * 50 >= 12000 )) && return 1
    done
}
if new_worker_accounted; then
    ok "accounting works in post-reload workers"
else
    fail "accounting lost after config reload"
fi

cleanup

if [[ $FAILS -eq 0 ]]; then
    echo "PASS"
    exit 0
fi
echo "FAIL ($FAILS failures)"
exit 1
