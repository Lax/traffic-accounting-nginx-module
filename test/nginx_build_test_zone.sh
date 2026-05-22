#!/bin/sh
# Usage: test/nginx_build_test_zone.sh [NGX_VER=1.30.1]
# Builds nginx with traffic-accounting module and accounting_zone enabled,
# validates config, sends multiple requests, and verifies aggregated logs.
set -e

NGX_VER="${1:-1.30.1}"
IMAGE="nginx-acc-zone-test:${NGX_VER}"

echo "=== [1/3] Build nginx ${NGX_VER} with accounting_zone ==="
docker build -f test/Dockerfile-zone \
  --build-arg "NGX_VER=${NGX_VER}" -t "${IMAGE}" .

echo "=== [2/3] Config validation ==="
docker run --rm --entrypoint /opt/nginx/sbin/nginx "${IMAGE}" -t

echo "=== [3/3] Start & request ==="
CID=$(docker run -d -p 8090:8080 "${IMAGE}")
sleep 8

# Send 3 requests to /index (same accounting_id) to verify accumulation
curl -s -o /dev/null -w "HTTP %{http_code}\n" http://localhost:8090/index || true
curl -s -o /dev/null -w "HTTP %{http_code}\n" http://localhost:8090/index || true
curl -s -o /dev/null -w "HTTP %{http_code}\n" http://localhost:8090/index || true

echo "=== Container logs ==="
docker logs "${CID}" 2>&1 | tail -20
echo ""

echo "=== Verifying zone aggregation markers ==="
LOGS=$(docker logs "${CID}" 2>&1)

HAS_WORKERS=$(echo "$LOGS" | grep -c 'workers:' || true)
HAS_PID=$(echo "$LOGS" | grep -c 'pid:' || true)
HAS_REQUESTS_3=$(echo "$LOGS" | grep -c 'requests:3' || true)

echo "  workers: prefix found: ${HAS_WORKERS}"
echo "  pid: prefix found: ${HAS_PID} (expected: 0 in period log)"
echo "  requests:3 found: ${HAS_REQUESTS_3}"

docker kill "${CID}" >/dev/null 2>&1 || true

if [ "$HAS_WORKERS" -gt 0 ] && [ "$HAS_REQUESTS_3" -gt 0 ]; then
    echo "=== PASS: zone aggregation verified ==="
else
    echo "=== FAIL: missing workers: prefix or accumulated request count ==="
    exit 1
fi
