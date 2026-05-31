test_v03() {
    local mode=$1

    if has_module "$mode" http; then
        local conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "accounting_interval 30;" "" "$conf"
        if assert_config_ok "$mode" "$conf"; then
            pass "V03a $mode: accounting_interval 30 passes"
        else
            fail "V03a $mode: accounting_interval 30 should pass"
        fi
        rm -f "$conf"
    else
        skip "V03a $mode: no http module"
    fi

    if has_module "$mode" http; then
        local conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "accounting_interval abc;" "" "$conf"
        if assert_config_fail "$mode" "$conf"; then
            pass "V03b $mode: accounting_interval abc rejected"
        else
            fail "V03b $mode: accounting_interval abc should be rejected"
        fi
        rm -f "$conf"
    else
        skip "V03b $mode: no http module"
    fi
}
