#!/usr/bin/env bash
#
# Functional test-suite runner.
#
# Usage: run-tests.sh <nginx-binary> <variant: both|http|stream> [test-name-filter]
#
# Each tests/t/*.sh is executed with NGINX_BIN and VARIANT in the environment.
# A test prints "ok - ..." lines and exits 0 on pass, non-zero on failure.

set -u

TESTS_DIR=$(cd "$(dirname "$0")" && pwd)
NGINX_BIN=${1:?usage: run-tests.sh <nginx-binary> <variant> [filter]}
VARIANT=${2:?variant: both|http|stream}
FILTER=${3:-}

export NGINX_BIN VARIANT TESTS_DIR
[[ -x "$NGINX_BIN" ]] || { echo "not executable: $NGINX_BIN" >&2; exit 2; }

PASS=0
FAIL=0
SKIP=0
FAILED_TESTS=()

for t in "$TESTS_DIR"/t/*.sh; do
    name=$(basename "$t" .sh)
    [[ -z "$FILTER" || "$name" == *"$FILTER"* ]] || continue
    echo "== $name"
    out=$(bash "$t" 2>&1)
    rc=$?
    echo "$out" | sed 's/^/   /'
    if echo "$out" | grep -q "^SKIP"; then
        echo "   -> SKIP"
        SKIP=$((SKIP+1))
    elif [[ $rc -eq 0 ]]; then
        echo "   -> PASS"
        PASS=$((PASS+1))
    else
        echo "   -> FAIL"
        FAIL=$((FAIL+1))
        FAILED_TESTS+=("$name")
    fi
done

echo
echo "total: pass=$PASS fail=$FAIL skip=$SKIP"
if [[ ${#FAILED_TESTS[@]} -gt 0 ]]; then
    echo "failed: ${FAILED_TESTS[*]}"
    exit 1
fi
exit 0
