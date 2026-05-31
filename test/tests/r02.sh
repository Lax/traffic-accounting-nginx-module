# R02: runtime — Stream session triggers accounting log
test_r02() {
    local mode=$1

    if ! has_module "$mode" stream; then
        skip "R02 $mode: no stream module"
        return
    fi

    # m3 loads two separate .so files that share global state → worker segfaults
    if [[ "$mode" == "m3" ]]; then
        skip "R02 $mode: separate .so conflict (SIGSEGV)"
        return
    fi

    local conf=$(mktemp /tmp/acc-XXXXXX.conf)
    render_conf "$mode" "" \
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
    cid=$(start_container "$mode" "$conf" "-p 9999:9999")
    sleep 1

    timeout 5 nc -w1 localhost 9999 || true
    sleep 3

    if assert_log_contains "$cid" "accounting_id:"; then
        pass "R02 $mode: Stream session produces accounting log"
    else
        fail "R02 $mode: Stream session should produce accounting log"
    fi

    stop_container "$cid"
    rm -f "$conf"
}
