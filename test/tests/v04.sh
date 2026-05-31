test_v04() {
    local mode=$1

    if ! has_module "$mode" http; then
        skip "V04 $mode: no http module"
        return
    fi

    local conf=$(mktemp /tmp/acc-XXXXXX.conf)
    render_conf "$mode" "accounting_log /dev/stderr;" "" "$conf"
    if assert_config_ok "$mode" "$conf"; then
        pass "V04 $mode: accounting_log /dev/stderr passes"
    else
        fail "V04 $mode: accounting_log /dev/stderr should pass"
    fi
    rm -f "$conf"
}
