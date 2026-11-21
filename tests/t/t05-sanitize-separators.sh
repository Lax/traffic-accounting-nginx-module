#!/usr/bin/env bash
# t05: id sanitization — the field separator '|' must not reach the exported
# format unescaped (it would forge/corrupt downstream parsing). '|' is
# deliverable by clients both via the URI query and via the Host header.
# Control/DEL bytes are rejected by nginx's own request parsers (400) — those
# requests must still be accounted (under __default__), never silently lost.
source "${TESTS_DIR}/lib/harness.sh"
require_variant http
new_rundir
PORT=$(pick_port)
CONF=$RUNDIR/nginx.conf
cat > "$CONF" <<EOF
worker_processes 1;
error_log $RUNDIR/error.log warn;
pid $RUNDIR/nginx.pid;
events { worker_connections 8192; }
http {
    access_log off;
    accounting on;
    accounting_log $RUNDIR/accounting.log;
    accounting_interval 1;
    server {
        listen 127.0.0.1:$PORT;
        location / { accounting_id \$arg_uid; return 200 "ok"; }
        location /host { accounting_id \$http_host; return 200 "ok"; }
    }
}
EOF

start_nginx "$CONF"
wait_port "$PORT" || { fail "nginx did not come up"; finish; }

# raw pipe in the URI query -> $arg_uid
printf 'GET /?uid=a\x7cb HTTP/1.1\r\nHost: t\r\nConnection: close\r\n\r\n' | raw_request "$PORT" >/dev/null
# raw pipe in the Host header -> $http_host (README's own example uses $http_host).
# How upstream treats this header varies across nginx releases: it used to be
# delivered to the module (id sanitized to p_q); starting in the 1.29/1.30 era
# nginx rejects the request itself with 400 ("client sent invalid host
# header").  Assert on observed behavior, never on version numbers: send the
# request once and branch the expectations on the response actually received.
host_resp=$(printf 'GET /host HTTP/1.1\r\nHost: p|q\r\nConnection: close\r\n\r\n' | raw_request "$PORT")
case "$host_resp" in
    "HTTP/1.1 400"*|"HTTP/1.0 400"*) host_rejected=1 ;;
    *)                               host_rejected=0 ;;
esac
# control byte and DEL: rejected by nginx itself with 400
printf 'GET /?uid=c\x7fd HTTP/1.1\r\nHost: t\r\nConnection: close\r\n\r\n' | raw_request "$PORT" >/dev/null
printf 'GET /?uid=e\x01f HTTP/1.1\r\nHost: t\r\nConnection: close\r\n\r\n' | raw_request "$PORT" >/dev/null
# percent-encoded pipe: $arg_* keeps escapes verbatim (format-safe bytes)
printf 'GET /?uid=p%%71%%7C%%72 HTTP/1.1\r\nHost: t\r\nConnection: close\r\n\r\n' | raw_request "$PORT" >/dev/null
# control group
http_get "$PORT" "/?uid=clean" >/dev/null

wait_acct 8 '[[ $(sum_field clean requests) -ge 1 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "query pipe sanitized to _" "a_b" "$(acct ids | grep -x 'a_b')"
if [[ $host_rejected -eq 1 ]]; then
    # upstream rejected the Host header itself: the request must still be
    # accounted (under __default__), just not under a piped id
    check_eq "invalid-host request still accounted under __default__" 3 "$(sum_field __default__ requests)"
else
    check_eq "host-header pipe sanitized to _" "p_q" "$(acct ids | grep -x 'p_q')"
    # the two parser-rejected (400) requests must not vanish from the books
    check_eq "rejected control-byte requests accounted under __default__" 2 "$(sum_field __default__ requests)"
fi
check_eq "percent escapes kept verbatim" "p%71%7C%72" "$(acct ids | grep -xF 'p%71%7C%72')"
check_eq "clean id unaffected" "clean" "$(acct ids | grep -x clean)"
# nothing may vanish or be double-booked: five probe requests + the clean one
check_eq "TOTAL requests conserved across buckets" 6 "$(sum_field '' requests)"

# byte-level: no exported id may contain a raw '|' or control/DEL byte.
# Inspect the RAW log lines and re-extract the id segment here: the parser
# in lib/parseacct.py splits fields on '|' first, so a leaked separator
# would be invisible through it.  The capture is anchored at the LAST
# '|requests:'/'|sessions:' field, so a leaked '|' inside the id lands
# inside the captured group instead of being swallowed as a field split.
BAD=$(python3 - "$ACCT_LOG" <<'EOF'
import re, sys
pat = re.compile(rb"accounting_id:(.*)\|(?:requests|sessions):")
bad = []
for raw in open(sys.argv[1], "rb"):
    m = pat.search(raw)
    if not m:
        continue
    ident = m.group(1)
    if b"|" in ident or any(b < 0x20 or b == 0x7f for b in ident):
        bad.append(ident.decode("utf-8", "replace"))
print("\n".join(bad))
EOF
)
check_eq "no raw separator/control bytes in any exported id" "" "$BAD"
finish
