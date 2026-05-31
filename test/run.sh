#!/bin/bash
# run.sh - Test suite entry point for traffic-accounting-nginx-module
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR"

# Source matrix and lib
source "$SCRIPT_DIR/matrix.sh"
source "$SCRIPT_DIR/lib.sh"

# Parse args
FILTER_MODE=""
FILTER_TEST=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode) FILTER_MODE="$2"; shift 2 ;;
        --test) FILTER_TEST="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        *) echo "Usage: $0 [--mode m1,m2] [--test v01,r01] [--skip-build]"; exit 1 ;;
    esac
done

# Build images if needed
if [[ "$SKIP_BUILD" != "1" ]]; then
    build_images
fi

# Determine which modes to run
MODES=""
if [[ "$FILTER_MODE" ]]; then
    for m in ${FILTER_MODE//,/ }; do MODES="$MODES $m"; done
else
    MODES="m1 m2 m3 m4"
fi

# Determine which tests to run
TESTS=""
if [[ "$FILTER_TEST" ]]; then
    for t in ${FILTER_TEST//,/ }; do TESTS="$TESTS $t"; done
else
    TESTS="v01 v02 v03 v04 v05 r01 r02 r03 e01"
fi

for mode in $MODES; do
    if [[ ! "${IMAGE[$mode]}" ]]; then
        echo "Unknown mode: $mode"
        exit 1
    fi

    echo ""
    echo "=============================================="
    echo " Mode: $mode - ${DESC[$mode]}"
    echo "=============================================="

    for tc in $TESTS; do
        tc_file="$SCRIPT_DIR/tests/${tc}.sh"
        if [[ -f "$tc_file" ]]; then
            source "$tc_file"
            "test_${tc}" "$mode"
        fi
    done
done

echo ""
echo "=============================================="
print_summary
exit $?
