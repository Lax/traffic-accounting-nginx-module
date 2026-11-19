#!/usr/bin/env bash
# t03: unattributed traffic — all four classes merge into __default__
#   1. location without accounting_id (config default)
#   2. empty variable id (/?uid=)
#   3. unset variable id (HTTP/1.0 without Host, $http_host not found)
#   4. organic id that equals the reserved word "__default__"
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
        location /noid { return 200 "ok"; }
    }
}
EOF

start_nginx "$CONF"
wait_port "$PORT"

# class 1: config-default bucket
for i in 1 2; do http_get "$PORT" /noid >/dev/null; done
# class 2: empty variable id
http_get "$PORT" "/?uid=" >/dev/null
# class 3: HTTP/1.0 request without Host header -> $http_host not found
printf 'GET /host HTTP/1.0\r\n\r\n' | raw_request "$PORT" >/dev/null
# class 4: organic id equal to the reserved word
for i in 1 2 3; do http_get "$PORT" "/?uid=__default__" >/dev/null; done
# control group: attributed traffic
http_get "$PORT" "/?uid=control" >/dev/null

wait_acct 8 '[[ $(sum_field __default__ requests 2>/dev/null || echo 0) -ge 7 ]]' \
    || { fail "no export appeared"; finish; }
stop_nginx_graceful

check_eq "__default__ merges all 4 classes (2+1+1+3)" 7 "$(sum_field __default__ requests)"
check_eq "control id counted separately" 1 "$(sum_field control requests)"
check_eq "only two ids exported" 2 "$(acct ids | wc -l)"
finish
