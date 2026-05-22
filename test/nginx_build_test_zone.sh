#!/bin/sh
# Usage: test/nginx_build_test_zone.sh [NGX_VER=1.30.1]
# Builds nginx with traffic-accounting module and accounting_zone enabled,
# validates config, sends multiple requests, and verifies aggregated logs.
set -e

NGX_VER="${1:-1.30.1}"
IMAGE="nginx-acc-zone-test:${NGX_VER}"

echo "=== [1/4] Build nginx ${NGX_VER} base ==="
BASE_IMAGE="${IMAGE}-base"
docker build -f test/Dockerfile.nginx \
  --build-arg "NGX_VER=${NGX_VER}" -t "${BASE_IMAGE}" .

echo "=== [2/4] Overlay zone test configs ==="
cat > /tmp/Dockerfile.zone-ovr << EOF
FROM ${BASE_IMAGE}
COPY test/zone-nginx.conf /opt/nginx/conf/nginx.conf
COPY test/zone-http.conf /opt/nginx/conf/http.conf
COPY test/zone-stream.conf /opt/nginx/conf/stream.conf
EOF
docker build -f /tmp/Dockerfile.zone-ovr -t "${IMAGE}" .
rm -f /tmp/Dockerfile.zone-ovr

echo "=== [3/4] Config validation ==="
docker run --rm --entrypoint /opt/nginx/sbin/nginx "${IMAGE}" -t

echo "=== [4/4] Start & request ==="
CID=$(docker run -d -p 8090:8080 "${IMAGE}")
sleep 3

# Send 3 requests early so they land in a period that gets rotated
curl -s -o /dev/null -w "HTTP %{http_code}\n" http://localhost:8090/index || true
curl -s -o /dev/null -w "HTTP %{http_code}\n" http://localhost:8090/index || true
curl -s -o /dev/null -w "HTTP %{http_code}\n" http://localhost:8090/index || true

# Wait for period rotation (interval=5, need 2 cycles: one to catch, one to log)
sleep 12

echo "=== Container logs ==="
docker logs "${CID}" 2>&1 | grep "accounting_id:" | tail -10
echo ""

echo "=== Verifying zone aggregation markers ==="
LOGS=$(docker logs "${CID}" 2>&1)

ACC_LOGS=$(echo "$LOGS" | grep 'accounting_id:' || true)
HAS_WORKERS=$(echo "$ACC_LOGS" | grep -c 'workers:' || true)
HAS_REQUESTS=$(echo "$ACC_LOGS" | grep -c 'requests:3' || true)

echo "  accounting log lines: $(echo "$ACC_LOGS" | wc -l)"
echo "  workers: in accounting log: ${HAS_WORKERS}"
echo "  requests:3 found: ${HAS_REQUESTS}"

docker kill "${CID}" >/dev/null 2>&1 || true

if [ "$HAS_WORKERS" -gt 0 ] && [ "$HAS_REQUESTS" -gt 0 ]; then
    echo "=== PASS: zone aggregation verified ==="
else
    echo "=== FAIL: missing workers: prefix or accumulated request count ==="
    echo "Full container logs:"
    echo "$LOGS"
    exit 1
fi
