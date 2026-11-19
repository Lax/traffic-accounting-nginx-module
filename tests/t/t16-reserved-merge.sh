#!/usr/bin/env bash
# t16: organic ids equal to reserved words merge into the reserved buckets
# (cap is 16: the enforced minimum)
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1 16; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT" || { fail "nginx did not come up"; finish; }

# fill the cap with 16 organics
for u in a b c d e f g h i j k l m n o p; do http_get "$PORT" "/?uid=$u" >/dev/null; done
# organic "__overflow__" + 2 excess organics all land in the overflow node
http_get "$PORT" "/?uid=__overflow__" >/dev/null
http_get "$PORT" "/?uid=q1" >/dev/null
http_get "$PORT" "/?uid=q2" >/dev/null
# organic "__default__" merges with unattributed traffic
http_get "$PORT" "/?uid=__default__" >/dev/null
http_get "$PORT" "/?uid=" >/dev/null
http_get "$PORT" "/noid" >/dev/null

wait_acct 8 '[[ $(sum_field __overflow__ requests 2>/dev/null || echo 0) -ge 3 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "overflow bucket: organic __overflow__ + q1 + q2" 3 "$(sum_field __overflow__ requests)"
check_eq "default bucket: organic __default__ + empty + no-directive" 3 "$(sum_field __default__ requests)"
check_eq "16 organics + 2 reserved = 18 ids" 18 "$(acct ids | wc -l)"
check_eq "reserved-word organic never splits into a second node" 0 "$(acct ids | grep -c '^__default__.*__default__' || true)"
finish
