#!/usr/bin/env bash
#
# Load conservation + export-stall measurement.
#
#   run-conservation.sh <nginx-bin> [--total N] [--interval S] [--max-metrics N]
#                       [--conns N] [--pipeline N] [--label NAME]
#
# Drives N requests, each with a unique accounting id, across at least two
# period rotations, then verifies EXACT conservation: the sum of exported
# per-id request counts must equal the number of responses received, and
# (when the cap allows) every id must be exported exactly once.
#
# Also reports the export "spread": wall-clock seconds between the first and
# last log line written per export batch. The pre-PR export loop restarted
# from the rbtree root after every node deletion (O(n^2)); with tens of
# thousands of distinct ids this shows up as a spread of many seconds and a
# client-side read gap of the same order.
#
# Exit 0 iff conservation is exact.

set -u

NGINX_BIN=${1:?usage: run-conservation.sh <nginx-bin> [options]}; shift
TOTAL=90000
INTERVAL=5
MAX_METRICS=1000000
CONNS=24
PIPELINE=64
LABEL=run

while [[ $# -gt 0 ]]; do
    case "$1" in
        --total) TOTAL=$2; shift 2;;
        --interval) INTERVAL=$2; shift 2;;
        --max-metrics) MAX_METRICS=$2; shift 2;;
        --conns) CONNS=$2; shift 2;;
        --pipeline) PIPELINE=$2; shift 2;;
        --label) LABEL=$2; shift 2;;
        *) echo "unknown option $1" >&2; exit 2;;
    esac
done

HERE=$(cd "$(dirname "$0")" && pwd)
[[ -x "$HERE/genload" ]] || (cd "$HERE" && cc -O2 -o genload genload.c) || { echo "genload build failed" >&2; exit 2; }

RUNDIR=$(mktemp -d /tmp/ngx-ta-load.XXXXXX)
ACCT_LOG="$RUNDIR/accounting.log"
PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')

CAP_LINE=""
[[ "$MAX_METRICS" != "0" ]] && CAP_LINE="    accounting_max_metrics $MAX_METRICS;"
cat > "$RUNDIR/nginx.conf" <<EOF
worker_processes 2;
error_log $RUNDIR/error.log warn;
pid $RUNDIR/nginx.pid;
events { worker_connections 16384; }
http {
    access_log off;
    accounting on;
    accounting_log $ACCT_LOG;
    accounting_interval $INTERVAL;
$CAP_LINE
    server {
        listen 127.0.0.1:$PORT;
        location / { accounting_id \$arg_uid; return 200 "ok"; }
    }
}
EOF

"$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/nginx.conf" -t -q || { echo "config test failed" >&2; exit 2; }
"$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/nginx.conf" || { echo "nginx failed to start" >&2; exit 2; }
sleep 1

# span at least two rotations
RUN_SECONDS=$(( INTERVAL * 2 + 2 ))
"$HERE/genload" 127.0.0.1 "$PORT" "$CONNS" "$PIPELINE" "$TOTAL" --id-mode unique --prefix c > "$RUNDIR/genload.out" 2>"$RUNDIR/genload.err" &
LOAD_PID=$!
sleep "$RUN_SECONDS"
kill -0 $LOAD_PID 2>/dev/null && wait $LOAD_PID
LOAD_RC=$?

"$NGINX_BIN" -p "$RUNDIR/" -c "$RUNDIR/nginx.conf" -s quit 2>/dev/null
for _ in $(seq 1 100); do
    [[ -f "$RUNDIR/nginx.pid" ]] || break
    kill -0 "$(cat $RUNDIR/nginx.pid)" 2>/dev/null || break
    sleep 0.2
done

RESULT=$(cat "$RUNDIR/genload.out" 2>/dev/null | tail -1)
OK_COUNT=$(sed -n 's/.*ok=\([0-9]*\).*/\1/p' <<<"$RESULT")
ERR_COUNT=$(sed -n 's/.*err=\([0-9]*\).*/\1/p' <<<"$RESULT")
RPS=$(sed -n 's/.*rps=\([0-9]*\).*/\1/p' <<<"$RESULT")
MAX_GAP=$(sed -n 's/.*max_read_gap_ms=\([0-9.]*\).*/\1/p' <<<"$RESULT")

EXPORTED=$(python3 "$HERE/../lib/parseacct.py" "$ACCT_LOG" sum --field requests 2>/dev/null || echo 0)
IDS=$(python3 "$HERE/../lib/parseacct.py" "$ACCT_LOG" ids 2>/dev/null | wc -l)
SPREAD=$(python3 "$HERE/../lib/parseacct.py" "$ACCT_LOG" spread 2>/dev/null | tail -1 | sed 's/worst_s=//')

echo "[$LABEL] label=$LABEL total=$TOTAL interval=${INTERVAL}s max_metrics=$MAX_METRICS"
echo "[$LABEL] client: ok=$OK_COUNT err=$ERR_COUNT rps=$RPS max_read_gap_ms=$MAX_GAP load_rc=$LOAD_RC"
echo "[$LABEL] export: requests_sum=$EXPORTED distinct_ids=$IDS worst_export_spread_s=$SPREAD"

conserved=1
[[ "$EXPORTED" == "$TOTAL" ]] || conserved=0
[[ "$OK_COUNT" == "$TOTAL" ]] || conserved=0
[[ "$LOAD_RC" == 0 ]] || conserved=0
if [[ $MAX_METRICS -ge $TOTAL && "$IDS" != "$TOTAL" ]]; then conserved=0; fi

echo "[$LABEL] dir=$RUNDIR"
if [[ $conserved -eq 1 ]]; then
    echo "[$LABEL] VERDICT: PASS (exact conservation)"
    exit 0
fi
echo "[$LABEL] VERDICT: FAIL (conservation broken)"
exit 1
