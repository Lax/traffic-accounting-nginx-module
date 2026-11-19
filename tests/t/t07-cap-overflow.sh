#!/usr/bin/env bash
# t07: accounting_max_metrics cap — ids beyond the cap land in __overflow__,
# with a one-time alert
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1 16; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

for i in $(seq -w 1 20); do http_get "$PORT" "/?uid=u$i" >/dev/null; done

wait_acct 8 '[[ $(sum_field __overflow__ requests 2>/dev/null || echo 0) -ge 4 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "16 organic ids exported" 16 "$(acct ids | grep -c '^u')"
check_eq "overflow holds exactly the 4 excess requests" 4 "$(sum_field __overflow__ requests)"
check_eq "17 ids in total (16 + overflow)" 17 "$(acct ids | wc -l)"
check_eq "one-time overflow alert" 1 "$(grep -c 'more than 16 accounting ids' "$ACCT_LOG")"
finish
