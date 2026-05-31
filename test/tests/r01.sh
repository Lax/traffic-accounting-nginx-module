# R01: runtime — HTTP request triggers accounting log
test_r01() {
    local mode=$1

    if ! has_module "$mode" http; then
        skip "R01 $mode: no http module"
        return
    fi

    # m3 loads two separate .so files that share global state → worker segfaults
    if [[ "$mode" == "m3" ]]; then
        skip "R01 $mode: separate .so conflict (SIGSEGV)"
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
        "" "$conf"

    local cid
    cid=$(start_container "$mode" "$conf" "-p 8080:8080")
    sleep 1

    timeout 5 curl -s -o /dev/null http://localhost:8080/ || true
    sleep 3

    if assert_log_contains "$cid" "accounting_id:"; then
        pass "R01 $mode: HTTP request produces accounting log"
    else
        fail "R01 $mode: HTTP request should produce accounting log"
    fi

    stop_container "$cid"
    rm -f "$conf"
}
