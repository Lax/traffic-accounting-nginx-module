test_v01() {
    local mode=$1

    # accounting on in http {} main conf
    if has_module "$mode" http; then
        local conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "accounting on;" "" "$conf"
        if assert_config_ok "$mode" "$conf"; then
            pass "V01 $mode: accounting on in http {} passes"
        else
            fail "V01 $mode: accounting on in http {} should pass"
        fi
        rm -f "$conf"
    else
        skip "V01 $mode: no http module"
    fi
}
