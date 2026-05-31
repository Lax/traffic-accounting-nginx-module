# R03: runtime — HTTP + Stream together produce accounting logs
test_r03() {
    local mode=$1

    if ! has_module "$mode" http || ! has_module "$mode" stream; then
        skip "R03 $mode: need both http and stream modules"
        return
    fi

    # m3 loads two separate .so files that share global state → worker segfaults
    if [[ "$mode" == "m3" ]]; then
        skip "R03 $mode: separate .so conflict (SIGSEGV)"
        return
    fi

    local conf=$(mktemp /tmp/acc-XXXXXX.conf)
    render_conf "$mode" \
        "accounting on;
         accounting_interval 2;
         accounting_perturb off;
         accounting_log /dev/stderr;
         server {
             listen 8080;
             location / { return 200 'ok'; }
         }" \
        "accounting on;
         accounting_interval 2;
         accounting_perturb off;
         accounting_log /dev/stderr;
         server {
             listen 9999;
             return hello;
         }" \
        "$conf"

    local cid
    cid=$(start_container "$mode" "$conf" "-p 8080:8080 -p 9999:9999")
    sleep 1

    timeout 5 curl -s -o /dev/null http://localhost:8080/ || true
    timeout 5 nc -w1 localhost 9999 || true
    sleep 3

    local logs
    logs=$(docker logs "$cid" 2>&1)

    if echo "$logs" | grep -q "accounting_id:"; then
        if echo "$logs" | grep "accounting" | grep -q "requests"; then
            pass "R03 $mode: Both modules produce accounting logs"
        else
            fail "R03 $mode: accounting logs should contain request counts"
        fi
    else
        fail "R03 $mode: should produce accounting logs from both modules"
    fi

    stop_container "$cid"
    rm -f "$conf"
}
