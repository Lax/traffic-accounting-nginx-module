# V02a: valid accounting_interval 30 accepted; V02b: invalid abc rejected
test_v02() {
    local mode=$1
    if has_module "$mode" http; then
        local conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "accounting_interval 30;" "" "$conf"
        if assert_config_ok "$mode" "$conf"; then
            pass "V02a $mode: accounting_interval 30 passes"
        else
            fail "V02a $mode: accounting_interval 30 should pass"
        fi
        rm -f "$conf"

        conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "accounting_interval abc;" "" "$conf"
        if assert_config_fail "$mode" "$conf"; then
            pass "V02b $mode: accounting_interval abc rejected"
        else
            fail "V02b $mode: accounting_interval abc should be rejected"
        fi
        rm -f "$conf"
    else
        skip "V02 $mode: no http module"
    fi
}
