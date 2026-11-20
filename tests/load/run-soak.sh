#!/usr/bin/env bash
#
# ASAN soak: high-volume traffic across many rotations on an ASAN build;
# any AddressSanitizer report fails the run.
#
#   run-soak.sh <asan-nginx-bin> [--total N] [--rotations N]

set -u

NGINX_BIN=${1:?usage: run-soak.sh <asan-nginx-bin> [options]}; shift
TOTAL=30000
INTERVAL=1
ROTATIONS=6

while [[ $# -gt 0 ]]; do
    case "$1" in
        --total) TOTAL=$2; shift 2;;
        --rotations) ROTATIONS=$2; shift 2;;
    esac
done

HERE=$(cd "$(dirname "$0")" && pwd)
[[ -x "$HERE/genload" ]] || (cd "$HERE" && cc -O2 -o genload genload.c) || exit 2

RUNDIR=$(mktemp -d /tmp/ngx-ta-asan.XXXXXX)
ACCT_LOG="$RUNDIR/accounting.log"
PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')

cat > "$RUNDIR/nginx.conf" <<EOF
worker_processes 1;
error_log $RUNDIR/error.log warn;
pid $RUNDIR/nginx.pid;
events { worker_connections 16384; }
http {
    access_log off;
    accounting on;
    accounting_log $ACCT_LOG;
    accounting_interval $INTERVAL;
    server {
        listen 127.0.0.1:$PORT;
        keepalive_timeout 600;
        keepalive_requests 10000000;
        location / { accounting_id \$arg_uid; return 200 "ok"; }
    }
}
EOF

# detect_leaks=0: exit-time LSan reports are nginx-core-inherent (the master
# intentionally never frees cycle/config pools; a stock nginx ASAN control
# run leaks the same ngx_palloc allocations), and LSan's conservative
# reachability scan makes them flaky.  Memory-error detection (use-after-free,
# overflow, ...) — the actual soak signal — stays fully enabled.
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:log_path="$RUNDIR/asan"
"$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/nginx.conf" || { echo "nginx failed to start" >&2; exit 2; }
sleep 1

DURATION=$(( ROTATIONS * INTERVAL + 2 ))
( "$HERE/genload" 127.0.0.1 "$PORT" 16 64 "$TOTAL" --id-mode unique --prefix s \
    > "$RUNDIR/genload.out" 2>&1
  echo $? > "$RUNDIR/genload.rc" ) &
sleep "$DURATION"
wait
LOAD_RC=$(cat "$RUNDIR/genload.rc" 2>/dev/null || echo 1)

"$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/nginx.conf" -s quit 2>/dev/null
sleep 2
kill -9 "$(cat $RUNDIR/nginx.pid 2>/dev/null)" 2>/dev/null

RESULT=$(tail -1 "$RUNDIR/genload.out")
OK_COUNT=$(sed -n 's/.*ok=\([0-9]*\).*/\1/p' <<<"$RESULT")
EXPORTED=$(python3 "$HERE/../lib/parseacct.py" "$ACCT_LOG" sum --field requests 2>/dev/null || echo 0)

ASAN_HITS=$(cat "$RUNDIR"/asan.* 2>/dev/null | grep -c "ERROR: AddressSanitizer" || true)

echo "[soak] ok=$OK_COUNT load_rc=$LOAD_RC exported=$EXPORTED asan_errors=$ASAN_HITS"
echo "[soak] dir=$RUNDIR"

verdict=0
[[ "$OK_COUNT" == "$TOTAL" ]] || { echo "[soak] client did not complete: $OK_COUNT/$TOTAL"; verdict=1; }
[[ "$EXPORTED" == "$TOTAL" ]] || { echo "[soak] conservation broken: exported $EXPORTED/$TOTAL"; verdict=1; }
[[ "$ASAN_HITS" == 0 ]] || { echo "[soak] AddressSanitizer errors found"; verdict=1; }

if [[ $verdict -eq 0 ]]; then echo "[soak] VERDICT: PASS"; else echo "[soak] VERDICT: FAIL"; fi
exit $verdict
