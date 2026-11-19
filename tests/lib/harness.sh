#!/usr/bin/env bash
#
# Shared harness for traffic-accounting functional tests.
# Required env: NGINX_BIN (path), VARIANT (both|http|stream).
# A test sources this file, builds a config, starts nginx, drives traffic and
# asserts on the exported accounting log.

TESTS_DIR=${TESTS_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
LIB_DIR="$TESTS_DIR/lib"

# --- test bookkeeping -------------------------------------------------------
_CHECKS=0
_FAILS=0

ok()   { _CHECKS=$((_CHECKS+1)); echo "    ok - $*"; }
fail() { _CHECKS=$((_CHECKS+1)); _FAILS=$((_FAILS+1)); echo "    NOT OK - $*" >&2; }
check_eq() { # desc expected actual
    if [[ "$2" == "$3" ]]; then ok "$1 ($3)"; else fail "$1: expected [$2], got [$3]"; fi
}
check_ge() { # desc expected-min actual
    if [[ "$3" -ge "$2" ]]; then ok "$1 ($3 >= $2)"; else fail "$1: expected >= $2, got $3"; fi
}
check_contains() { # desc haystack-file needle
    if grep -q -- "$3" "$2" 2>/dev/null; then ok "$1"; else fail "$1: [$3] not found in $2"; fi
}
finish() {
    if [[ $_FAILS -eq 0 ]]; then
        echo "PASS ($_CHECKS checks)"
        exit 0
    fi
    echo "FAIL ($_FAILS/$_CHECKS checks failed)"
    exit 1
}

# --- environment ------------------------------------------------------------
require_variant() { # skip unless variant matches http-part/stream-part presence
    case "$1" in
        http)  [[ "$VARIANT" == http || "$VARIANT" == both ]] || { echo "SKIP (needs http)"; exit 0; };;
        stream) [[ "$VARIANT" == stream || "$VARIANT" == both ]] || { echo "SKIP (needs stream)"; exit 0; };;
    esac
}

new_rundir() {
    RUNDIR=$(mktemp -d /tmp/ngx-ta-test.XXXXXX)
    ACCT_LOG="$RUNDIR/accounting.log"
    ERR_LOG="$RUNDIR/error.log"
    trap 'cleanup_nginx 2>/dev/null; rm -rf "$RUNDIR"' EXIT
}

pick_port() {
    python3 - <<'EOF'
import socket
s = socket.socket()
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
EOF
}

# --- nginx lifecycle ---------------------------------------------------------
start_nginx() { # $1 = conf file
    "$NGINX_BIN" -p "$RUNDIR/" -c "$1" -t >/dev/null 2>&1 \
        || { "$NGINX_BIN" -p "$RUNDIR/" -c "$1" -t; fail "nginx -t failed"; exit 1; }
    "$NGINX_BIN" -p "$RUNDIR/" -c "$1" || { fail "nginx failed to start"; exit 1; }
}

wait_port() { # $1 port [$2 timeout=10]
    local port=$1 t=${2:-10} tries=0
    until (exec 3<>"/dev/tcp/127.0.0.1/$port") 2>/dev/null; do
        sleep 0.2; tries=$((tries + 1))
        if (( tries * 2 >= t * 10 )); then echo "port $port never opened" >&2; return 1; fi
    done
    return 0
}

stop_nginx_graceful() { # nginx -s quit, wait up to 15s, then TERM
    local pid
    "$NGINX_BIN" -p "$RUNDIR/" -c "$CONF" -s quit >/dev/null 2>&1 || true
    for _ in $(seq 1 150); do
        [[ -f "$RUNDIR/nginx.pid" ]] || break
        pid=$(cat "$RUNDIR/nginx.pid" 2>/dev/null) || break
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.1
    done
    pid=$(cat "$RUNDIR/nginx.pid" 2>/dev/null || true)
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        kill -TERM "$pid" 2>/dev/null || true
        sleep 0.5
        kill -9 "$pid" 2>/dev/null || true
    fi
}

cleanup_nginx() {
    local pid
    pid=$(cat "$RUNDIR/nginx.pid" 2>/dev/null || true)
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        kill -TERM "$pid" 2>/dev/null || true
        sleep 0.3
        kill -9 "$pid" 2>/dev/null || true
    fi
    return 0
}

# --- config boilerplate -------------------------------------------------------
http_conf_open() { # $1 port $2 interval [$3 max_metrics] -> stdout (http { ... open)
    cat <<EOF
worker_processes ${WORKERS:-1};
error_log $RUNDIR/error.log warn;
pid $RUNDIR/nginx.pid;
events { worker_connections 8192; }
http {
    access_log off;
    accounting on;
    accounting_log $RUNDIR/accounting.log;
    accounting_interval $2;
EOF
    [[ -n "${3:-}" ]] && echo "    accounting_max_metrics $3;"
    cat <<EOF
    server {
        listen 127.0.0.1:$1;
        location = /fixed { accounting_id fixed_id; return 200 "ok"; }
        location / { accounting_id \$arg_uid; return 200 "ok"; }
        location /noid { return 200 "ok"; }
    }
EOF
}
http_conf_close() { echo "}"; }

stream_conf() { # $1 port $2 interval [$3 max_metrics] -> full stream-only conf on stdout
    cat <<EOF
worker_processes ${WORKERS:-1};
error_log $RUNDIR/error.log warn;
pid $RUNDIR/nginx.pid;
events { worker_connections 8192; }
stream {
    accounting on;
    accounting_log $RUNDIR/accounting.log;
    accounting_interval $2;
EOF
    [[ -n "${3:-}" ]] && echo "    accounting_max_metrics $3;"
    cat <<EOF
    server {
        listen 127.0.0.1:$1;
        return "ok";
    }
}
EOF
}

# --- traffic helpers ----------------------------------------------------------
http_get() { # $1 port $2 path [$3.. extra curl args]
    local port=$1 path=$2; shift 2
    curl -s -o /dev/null --max-time 5 -w '%{http_code}' "$@" "http://127.0.0.1:$port$path"
}

raw_request() { # $1 port ; request bytes on stdin -> response bytes on stdout
    python3 "$LIB_DIR/rawreq.py" 127.0.0.1 "$1"
}

tcp_once() { # $1 port ; open, read banner, close (one stream session)
    ( exec 3<>/dev/tcp/127.0.0.1/$1 && timeout 2 head -c 2 <&3 >/dev/null ) 2>/dev/null
}

# for stream tests: any TCP probe would itself be counted as a session, so
# wait on the master pid instead of connecting to the service port
wait_master() { # [$1 timeout=10]
    local t=${1:-10} tries=0
    while [[ ! -s "$RUNDIR/nginx.pid" ]]; do
        sleep 0.2; tries=$((tries + 1))
        (( tries * 2 >= t * 10 )) && { echo "nginx pid never appeared" >&2; return 1; }
    done
    sleep 0.5   # let workers bind their listen sockets
    return 0
}

# --- accounting log assertions --------------------------------------------------
# wait until the accounting log satisfies a condition (eval'd) or timeout
wait_acct() { # $1 timeout-seconds $2.. shell condition using $ACCT_LOG
    local t=$1; shift
    local cond=$*
    local tries=0
    while :; do
        if eval "$cond" 2>/dev/null; then return 0; fi
        sleep 0.25; tries=$((tries + 1))
        if (( tries * 25 >= t * 100 )); then return 1; fi
    done
}

acct() { python3 "$LIB_DIR/parseacct.py" "$ACCT_LOG" "$@"; }

sum_field() { # $1 id ('' = all) $2 field (requests/sessions/bytes_in/...)
    if [[ -z "$1" ]]; then acct sum --field "$2"; else acct sum --id "$1" --field "$2"; fi
}
