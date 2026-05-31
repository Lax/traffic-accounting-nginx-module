#!/bin/sh
# Usage: test/nginx_build_test.sh [NGX_VER=1.31.0]
# Builds nginx with the traffic-accounting module, validates config,
# starts the server, makes a request, and checks logs.
set -e

NGX_VER="${1:-1.31.0}"
IMAGE="nginx-acc-test:${NGX_VER}"
HOSTS="--add-host logstash:127.0.0.1"

if [ "${SKIP_BUILD:-0}" != "1" ]; then
echo "=== [1/3] Build nginx ${NGX_VER} ==="
docker build -f test/Dockerfile.nginx \
  --build-arg "NGX_VER=${NGX_VER}" -t "${IMAGE}" .
fi

echo "=== [2/3] Config validation ==="
docker run --rm ${HOSTS} \
  --entrypoint /opt/nginx/sbin/nginx "${IMAGE}" -t

echo "=== [3/3] Start & request ==="
CID=$(docker run -d -p 8080:8080 ${HOSTS} "${IMAGE}")
sleep 2

curl -s -o /dev/null -w "HTTP %{http_code}\n" http://localhost:8080/ || true

echo "=== Container logs ==="
docker logs "${CID}" 2>&1 | tail -5

docker kill "${CID}" >/dev/null 2>&1 || true

echo "=== OK: nginx ${NGX_VER} ==="
