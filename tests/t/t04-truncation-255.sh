#!/usr/bin/env bash
# t04: ids longer than 255 bytes are truncated to 255 and still accounted
# (they used to be dropped silently at export time = accounting bypass)
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

LONG_A=$(printf 'a%.0s' $(seq 1 300))
LONG_B=$(printf 'b%.0s' $(seq 1 300))
EXP_A=$(printf 'a%.0s' $(seq 1 255))
EXP_B=$(printf 'b%.0s' $(seq 1 255))

http_get "$PORT" "/?uid=$LONG_A" >/dev/null
http_get "$PORT" "/?uid=$LONG_B" >/dev/null
http_get "$PORT" "/?uid=$LONG_A" >/dev/null
http_get "$PORT" "/?uid=normal" >/dev/null

wait_acct 8 '[[ $(sum_field normal requests) -ge 1 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "300-byte id truncated to 255" "$EXP_A" "$(acct ids | grep '^a\{255\}$')"
check_eq "distinct long ids remain distinct after truncation" "$EXP_B" "$(acct ids | grep '^b\{255\}$')"
check_eq "truncated id counted with both requests" 2 "$(acct sum --id "$EXP_A" --field requests)"
check_eq "normal id unaffected" 1 "$(sum_field normal requests)"
finish
