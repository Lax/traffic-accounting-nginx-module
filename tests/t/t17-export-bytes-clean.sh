#!/usr/bin/env bash
# t17: byte-level export cleanliness — no NUL byte inside exported lines.
# Old builds included the string terminator in the exported length, writing
# a trailing NUL before the newline on every line, which corrupts downstream
# parsers (the value of the last field gains a \x00 suffix).
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
{ http_conf_open "$PORT" 1; http_conf_close; } > "$CONF"

start_nginx "$CONF"
wait_port "$PORT" || { fail "nginx did not come up"; finish; }

for i in $(seq 1 5); do http_get "$PORT" /fixed >/dev/null; done

wait_acct 8 '[[ $(sum_field fixed_id requests 2>/dev/null || echo 0) -ge 5 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

NULS=$(python3 -c "
data = open('$ACCT_LOG','rb').read()
print(data.count(b'\x00'))")
check_eq "zero NUL bytes in exported accounting log" 0 "$NULS"

# every accounting line must end with a plain newline
BAD_TAIL=$(python3 -c "
bad = 0
for line in open('$ACCT_LOG','rb').read().splitlines():
    if line.startswith(b'20') and b'accounting_id:' in line and line.endswith(b'\x00'):
        bad += 1
print(bad)")
check_eq "no line ends with a NUL before the newline" 0 "$BAD_TAIL"
finish
