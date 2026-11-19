#!/usr/bin/env bash
# t06: hash-collision family — ids that collide in ngx_hash_key_lc (case
# variants) but differ byte-wise must stay separate nodes with exact counts.
# The old lookup used reverse strncmp while insert used forward memcmp, so
# colliding ids could miss and insert duplicates of the same name.
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT"

# interleave the colliding family so lookups race against each other
for i in 1 2 3 4; do
    http_get "$PORT" "/?uid=Foo"  >/dev/null
    http_get "$PORT" "/?uid=fOo"  >/dev/null
    http_get "$PORT" "/?uid=FOO"  >/dev/null
done

wait_acct 8 '[[ $(sum_field Foo requests 2>/dev/null || echo 0) -ge 4 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "Foo counted exactly"      4 "$(sum_field Foo requests)"
check_eq "fOo counted exactly"      4 "$(sum_field fOo requests)"
check_eq "FOO counted exactly"      4 "$(sum_field FOO requests)"
check_eq "three distinct ids"       3 "$(acct ids | wc -l)"
check_eq "no duplicate export lines" 3 "$(grep -c 'accounting_id:' "$ACCT_LOG")"
finish
