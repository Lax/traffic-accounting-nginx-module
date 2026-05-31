PASS=0; FAIL=0; SKIP=0
GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; NC='\033[0m'

pass() { PASS=$((PASS+1)); echo -e "  ${GREEN}PASS${NC} $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC} $1"; }
skip() { SKIP=$((SKIP+1)); echo -e "  ${YELLOW}SKIP${NC} $1"; }

build_images() {
    echo "=== Building test image ==="
    docker build -f test/Dockerfile.test -t acc-test:rt .
}

has_module() {
    local mode=$1 mod=$2
    [[ " ${MODS[$mode]} " == *" $mod "* ]]
}

render_conf() {
    local mode=$1 http_block=$2 stream_block=$3 out=$4
    {
        echo "${LOAD[$mode]}"
        echo "worker_processes 1;"
        echo "error_log /dev/stderr;"
        echo "pid /tmp/nginx.pid;"
        echo "events { worker_connections 64; }"
        if [[ "$http_block" ]]; then
            echo "http {"
            echo "  access_log off;"
            echo "  default_type application/octet-stream;"
            echo "  $http_block"
            echo "}"
        fi
        if [[ "$stream_block" ]]; then
            echo "stream {"
            echo "  $stream_block"
            echo "}"
        fi
    } > "$out"
}

assert_config_ok() {
    local mode=$1 conf=$2
    local img=${IMAGE[$mode]}
    docker run --rm -v "$conf:/opt/nginx/conf/nginx.conf:ro" \
        "$img" /opt/nginx/sbin/nginx -t -c /opt/nginx/conf/nginx.conf 2>/dev/null
}

assert_config_fail() {
    local mode=$1 conf=$2
    local img=${IMAGE[$mode]}
    ! docker run --rm -v "$conf:/opt/nginx/conf/nginx.conf:ro" \
        "$img" /opt/nginx/sbin/nginx -t -c /opt/nginx/conf/nginx.conf 2>/dev/null
}

start_container() {
    local mode=$1 conf=$2 ports=$3
    local img=${IMAGE[$mode]}
    docker run -d $ports \
        -v "$conf:/opt/nginx/conf/nginx.conf:ro" \
        "$img" /opt/nginx/sbin/nginx -g "daemon off;" -c /opt/nginx/conf/nginx.conf
}

stop_container() {
    local cid=$1
    docker kill "$cid" >/dev/null 2>&1 || true
    docker rm "$cid" >/dev/null 2>&1 || true
}

assert_log_contains() {
    local cid=$1 pattern=$2
    docker logs "$cid" 2>&1 | grep -q "$pattern"
}

print_summary() {
    echo ""
    echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped ==="
    return $FAIL
}
