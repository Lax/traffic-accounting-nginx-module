#!/bin/bash
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"
source "$SCRIPT_DIR/matrix.sh"
source "$SCRIPT_DIR/lib.sh"

FILTER_MODE=""; FILTER_TEST=""; SKIP_BUILD=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode) FILTER_MODE="$2"; shift 2 ;;
        --test) FILTER_TEST="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        *) echo "Usage: $0 [--mode m1,m2] [--test v01,r01] [--skip-build]"; exit 1 ;;
    esac
done

if [[ "$SKIP_BUILD" != "1" ]]; then
    build_images
fi

MODES=""
if [[ "$FILTER_MODE" ]]; then
    for m in ${FILTER_MODE//,/ }; do MODES="$MODES $m"; done
else
    MODES="m1 m2 m3"
fi

TESTS=""
if [[ "$FILTER_TEST" ]]; then
    for t in ${FILTER_TEST//,/ }; do TESTS="$TESTS $t"; done
fi

for mode in $MODES; do
    if [[ ! "${IMAGE[$mode]}" ]]; then
        echo "Unknown mode: $mode"; exit 1
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

echo ""; echo "=============================================="
print_summary
exit $?
