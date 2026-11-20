#!/usr/bin/env bash
# t08: the reserved __default__ bucket is exempt from the cap (it always
# gets its own node, even after the cap is full)
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1 16; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

# fill the cap with 16 organic ids
for i in $(seq -w 1 16); do http_get "$PORT" "/?uid=u$i" >/dev/null; done
# unattributed traffic past the cap must still get the __default__ node
for i in 1 2 3; do http_get "$PORT" "/?uid=" >/dev/null; done
# two more organics go to overflow
http_get "$PORT" "/?uid=v1" >/dev/null
http_get "$PORT" "/?uid=v2" >/dev/null

wait_acct 8 '[[ $(sum_field __overflow__ requests 2>/dev/null || echo 0) -ge 2 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "__default__ counted past cap" 3 "$(sum_field __default__ requests)"
check_eq "overflow gets the 2 excess" 2 "$(sum_field __overflow__ requests)"
check_eq "16 organic + 2 reserved = 18 ids" 18 "$(acct ids | wc -l)"
finish
