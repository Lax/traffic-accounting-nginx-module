# V05a: accounting_id at http server+location; V05b: accounting_id in stream server
test_v05() {
    local mode=$1

    if has_module "$mode" http; then
        local conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" \
            "accounting_id 'GLOBAL'; server { listen 8080; location / { return 200; accounting_id 'LOCAL'; } }" \
            "" "$conf"
        if assert_config_ok "$mode" "$conf"; then
            pass "V05a $mode: accounting_id at server+location passes"
        else
            fail "V05a $mode: accounting_id at server+location should pass"
        fi
        rm -f "$conf"
    else
        skip "V05a $mode: no http module"
    fi

    if has_module "$mode" stream; then
        local conf=$(mktemp /tmp/acc-XXXXXX.conf)
        render_conf "$mode" "" \
            "accounting_id 'STREAMID'; server { listen 9999; return hello; }" \
            "$conf"
        if assert_config_ok "$mode" "$conf"; then
            pass "V05b $mode: accounting_id in stream server passes"
        else
            fail "V05b $mode: accounting_id in stream server should pass"
        fi
        rm -f "$conf"
    else
        skip "V05b $mode: no stream module"
    fi
}
