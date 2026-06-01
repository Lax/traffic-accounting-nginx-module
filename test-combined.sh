#!/bin/sh
set -e

echo "=== Step 1: Build all test images ==="
docker build -f Dockerfile.test --target test-http-stream -t ta-test-http-stream .
docker build -f Dockerfile.test --target test-http-only   -t ta-test-http-only .
docker build -f Dockerfile.test --target test-stream-only -t ta-test-stream-only

echo ""
echo "==========================================="
echo " Test 1: nginx(http+stream) + combined.so "
echo "==========================================="
docker run --rm ta-test-http-stream 2>&1 &
pid=$!
sleep 2
if kill -0 $pid 2>/dev/null; then
    echo ">>> [PASS] nginx started successfully (http+stream + combined)"
    kill $pid 2>/dev/null; wait $pid 2>/dev/null
else
    wait $pid 2>/dev/null
    echo ">>> [FAIL] nginx failed to start"
fi

echo ""
echo "==========================================="
echo " Test 2: nginx(http-only) + combined.so  "
echo "==========================================="
docker run --rm ta-test-http-only 2>&1 &
pid=$!
sleep 2
if kill -0 $pid 2>/dev/null; then
    echo ">>> [FAIL] nginx should NOT have started"
    kill $pid 2>/dev/null; wait $pid 2>/dev/null
else
    wait $pid 2>/dev/null
    echo ">>> [PASS] nginx correctly rejected combined.so on http-only build"
fi

echo ""
echo "==========================================="
echo " Test 3: nginx(stream-only) + combined.so"
echo "==========================================="
docker run --rm ta-test-stream-only 2>&1 &
pid=$!
sleep 2
if kill -0 $pid 2>/dev/null; then
    echo ">>> [FAIL] nginx should NOT have started"
    kill $pid 2>/dev/null; wait $pid 2>/dev/null
else
    wait $pid 2>/dev/null
    echo ">>> [PASS] nginx correctly rejected combined.so on stream-only build"
fi
