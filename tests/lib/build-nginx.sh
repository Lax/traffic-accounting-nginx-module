#!/usr/bin/env bash
#
# Build a stock nginx with the traffic-accounting module linked in,
# from an arbitrary module source tree (so the same harness can test any
# branch/commit without touching the working tree).
#
# Usage:
#   build-nginx.sh -s <module-src-dir> -o <out-dir> [-v both|http|stream]
#                  [--nginx-version 1.30.7] [--nginx-src <dir>] [--asan]
#                  [--werror] [--cc <compiler>] [--with-compat] [--dynamic]
#                  [-j N]
#
# Result: $out/nginx (binary), $out/nginx.version
#         (plus $out/*.so when --dynamic; logs: $out/configure.log, $out/make.log)
# With --dynamic: also copies $out/*.so (the built dynamic modules) and the
#   binary is linked against nothing extra -- load the .so files via
#   load_module in the config to use the module.
# --nginx-src accepts both release tarballs (top-level ./configure) and git
#   checkouts (./auto/configure).
# The build fails (non-zero exit) on any compiler warning when --werror is set.

set -euo pipefail

MODULE_SRC=""
OUT=""
VARIANT=both
NGINX_VERSION=${NGINX_VERSION:-1.30.7}
NGINX_SRC=""
ASAN=0
WERROR=0
CC=""
COMPAT=0
DYNAMIC=0
JOBS=${JOBS:-$(nproc)}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -s) MODULE_SRC=$2; shift 2;;
        -o) OUT=$2; shift 2;;
        -v) VARIANT=$2; shift 2;;
        --nginx-version) NGINX_VERSION=$2; shift 2;;
        --nginx-src) NGINX_SRC=$2; shift 2;;
        --asan) ASAN=1; shift;;
        --werror) WERROR=1; shift;;
        --cc) CC=$2; shift 2;;
        --with-compat) COMPAT=1; shift;;
        --dynamic) DYNAMIC=1; shift;;
        -j) JOBS=$2; shift 2;;
        -j*) JOBS=${1#-j}; shift;;
        *) echo "unknown option: $1" >&2; exit 2;;
    esac
done

[[ -d "$MODULE_SRC" && -f "$MODULE_SRC/config" ]] || { echo "invalid module src: $MODULE_SRC" >&2; exit 2; }
[[ -n "$OUT" ]] || { echo "missing -o" >&2; exit 2; }
case "$VARIANT" in
    both|http|stream) ;;
    *) echo "variant must be both|http|stream" >&2; exit 2;;
esac

MODULE_SRC=$(cd "$MODULE_SRC" && pwd)
CACHE_DIR=${NGX_TA_CACHE:-$HOME/.cache/traffic-accounting-tests}
NGINX_SRC_CACHE="$CACHE_DIR/nginx-$NGINX_VERSION"
TARBALL="$CACHE_DIR/nginx-$NGINX_VERSION.tar.gz"

mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

prepare_nginx_cache() {
    # serialize cache setup across concurrent invocations
    (
        flock -x 9
        if [[ ! -f "$NGINX_SRC_CACHE/src/core/nginx.c" ]]; then
            if [[ ! -f "$TARBALL" ]]; then
                echo "downloading nginx $NGINX_VERSION ..." >&2
                curl -fsSL -o "$TARBALL.part.$$" \
                    "https://nginx.org/download/nginx-$NGINX_VERSION.tar.gz"
                mv "$TARBALL.part.$$" "$TARBALL"
            fi
            # extract into a private dir, then move into place atomically
            local_tmp=$(mktemp -d "$CACHE_DIR/.extract.XXXXXX")
            tar -xzf "$TARBALL" -C "$local_tmp"
            if [[ ! -e "$NGINX_SRC_CACHE" ]]; then
                mv "$local_tmp/nginx-$NGINX_VERSION" "$NGINX_SRC_CACHE"
            fi
            rm -rf "$local_tmp"
        fi
    ) 9>"$CACHE_DIR/.lock"
}

if [[ -n "$NGINX_SRC" ]]; then
    # caller-provided nginx source tree (e.g. a mainline-tip snapshot);
    # bypass the version-keyed cache entirely
    NGINX_SRC=$(cd "$NGINX_SRC" && pwd)
    [[ -f "$NGINX_SRC/src/core/nginx.c" && ( -f "$NGINX_SRC/configure" || -f "$NGINX_SRC/auto/configure" ) ]] \
        || { echo "invalid nginx src dir: $NGINX_SRC (need src/core/nginx.c and configure or auto/configure)" >&2; exit 2; }
    NGINX_SRC_CACHE="$NGINX_SRC"
else
    mkdir -p "$CACHE_DIR"
    prepare_nginx_cache
fi

BUILD_TREE="$OUT/nginx-build"
rm -rf "$BUILD_TREE"
cp -a "$NGINX_SRC_CACHE" "$BUILD_TREE"

case "$VARIANT" in
    both)   VARIANT_OPTS=(--with-stream);;
    http)   VARIANT_OPTS=();;
    stream) VARIANT_OPTS=(--without-http --with-stream);;
esac

CC_OPT=""
LD_OPT=""
if [[ $ASAN -eq 1 ]]; then
    CC_OPT="-fsanitize=address -fno-omit-frame-pointer -g -O1"
    LD_OPT="-fsanitize=address"
fi
if [[ $WERROR -eq 1 ]]; then
    CC_OPT="$CC_OPT -Werror"
fi

CC_OPT_ARR=()
if [[ -n "$CC" ]]; then
    CC_OPT_ARR=(--with-cc="$CC")
fi
ADDON_MODE=--add-module
if [[ $DYNAMIC -eq 1 ]]; then
    ADDON_MODE=--add-dynamic-module
fi
COMPAT_OPT=()
if [[ $COMPAT -eq 1 ]]; then
    COMPAT_OPT=(--with-compat)
fi

# release tarballs ship ./configure; git checkouts only ./auto/configure
CONF=./configure
[[ -f "$BUILD_TREE/configure" ]] || CONF=./auto/configure

echo "configuring nginx $NGINX_VERSION [$VARIANT] asan=$ASAN werror=$WERROR cc=${CC:-default} compat=$COMPAT dynamic=$DYNAMIC" >&2
(cd "$BUILD_TREE" && $CONF \
    --prefix="$OUT/nginx-prefix" \
    "${CC_OPT_ARR[@]}" \
    --with-cc-opt="$CC_OPT" \
    --with-ld-opt="$LD_OPT" \
    "${VARIANT_OPTS[@]}" \
    "${COMPAT_OPT[@]}" \
    "$ADDON_MODE=$MODULE_SRC" \
) > "$OUT/configure.log" 2>&1 || { tail -40 "$OUT/configure.log" >&2; exit 1; }

echo "building ..." >&2
if ! (cd "$BUILD_TREE" && make -j"$JOBS" >/dev/null); then
    echo "BUILD FAILED - see $OUT/make.log" >&2
    (cd "$BUILD_TREE" && make -j1) > "$OUT/make.log" 2>&1 || true
    tail -60 "$OUT/make.log" >&2
    exit 1
fi

cp "$BUILD_TREE/objs/nginx" "$OUT/nginx"
if [[ $DYNAMIC -eq 1 ]]; then
    shopt -s nullglob
    so_files=("$BUILD_TREE"/objs/*.so)
    shopt -u nullglob
    if [[ ${#so_files[@]} -eq 0 ]]; then
        echo "no .so files produced by the dynamic build" >&2
        exit 1
    fi
    cp "${so_files[@]}" "$OUT/"
fi
"$OUT/nginx" -V 2>&1 | head -3 > "$OUT/nginx.version"
echo "OK: $OUT/nginx" >&2
