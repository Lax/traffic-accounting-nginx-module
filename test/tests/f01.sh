# F01: fix — combined .so with only one block configured, no segfault
test_f01() {
    local mode=$1
    # F01: verify process_init NULL check fix
    # M1 (combined .so) with only http {} block and no stream {} block
    # should NOT crash the worker process
    if ! has_module "$mode" http; then
        skip "F01 $mode: no http module"
        return
    fi
    local conf=$(mktemp /tmp/acc-XXXXXX.conf)
    render_conf "$mode" \
        "accounting on;
         accounting_interval 60;
         accounting_perturb off;
         accounting_log /dev/stderr;
         server {
             listen 8080;
             location / { return 200 'ok'; }
         }" \
        "" "$conf"
    local cid
    cid=$(start_container "$mode" "$conf" "-p 8080:8080")
    sleep 2
    # check container is still running (worker didn't crash)
    local status
    status=$(docker inspect "$cid" --format '{{.State.Status}}' 2>/dev/null || echo "missing")
    if [[ "$status" == "running" ]]; then
        timeout 3 curl -s -o /dev/null http://localhost:8080/ || true
        if assert_log_contains "$cid" "start http traffic accounting"; then
            pass "F01 $mode: combined .so with http-only config works (no segfault)"
        else
            fail "F01 $mode: expected accounting startup message"
        fi
    else
        fail "F01 $mode: container $status, worker likely crashed (missing NULL check)"
    fi
    stop_container "$cid"
    rm -f "$conf"
}
