test_v02() {
    local mode=$1

    # accounting on in stream {} main conf
    if has_module "$mode" stream; then
        local conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "" "accounting on;" "$conf"
        if assert_config_ok "$mode" "$conf"; then
            pass "V02 $mode: accounting on in stream {} passes"
        else
            fail "V02 $mode: accounting on in stream {} should pass"
        fi
        rm -f "$conf"
    else
        skip "V02 $mode: no stream module"
    fi
}
