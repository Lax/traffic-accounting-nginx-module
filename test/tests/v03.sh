# V03a: valid accounting_perturb on accepted; V03b: invalid abc rejected
test_v03() {
    local mode=$1
    if has_module "$mode" http; then
        local conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "accounting_perturb on;" "" "$conf"
        if assert_config_ok "$mode" "$conf"; then
            pass "V03a $mode: accounting_perturb on passes"
        else
            fail "V03a $mode: accounting_perturb on should pass"
        fi
        rm -f "$conf"

        conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "accounting_perturb abc;" "" "$conf"
        if assert_config_fail "$mode" "$conf"; then
            pass "V03b $mode: accounting_perturb abc rejected"
        else
            fail "V03b $mode: accounting_perturb abc should be rejected"
        fi
        rm -f "$conf"
    else
        skip "V03 $mode: no http module"
    fi
}
