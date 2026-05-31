test_e01() {
    local mode=$1

    if ! has_module "$mode" http; then
        skip "E01 $mode: no http module"
        return
    fi

    # Duplicate load_module of the same .so should fail
    local conf=$(mktemp /tmp/acc-XXXXXX.conf)
    {
        echo "${LOAD[$mode]}"
        echo "${LOAD[$mode]}"
        echo "worker_processes 1;"
        echo "error_log /dev/stderr;"
        echo "events { worker_connections 64; }"
        echo "http { access_log off; default_type application/octet-stream; accounting on; }"
    } > "$conf"

    if assert_config_fail "$mode" "$conf"; then
        pass "E01 $mode: duplicate load_module rejected"
    else
        fail "E01 $mode: duplicate load_module should be rejected"
    fi
    rm -f "$conf"
}
