#!/usr/bin/env bash

set -euo pipefail

NOVA_SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NOVA_BUILD_ROOT="$NOVA_SCRIPT_DIR/build"
NOVA_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET:-x64-linux-dynamic}

if [[ -z "${VCPKG_ROOT:-}" ]]; then
    echo "VCPKG_ROOT must point to a vcpkg checkout" >&2
    exit 1
fi

NOVA_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
if [[ ! -f "$NOVA_TOOLCHAIN_FILE" ]]; then
    echo "vcpkg toolchain not found: $NOVA_TOOLCHAIN_FILE" >&2
    exit 1
fi

build_config() {
    local nova_build_type=$1
    local nova_build_name=${nova_build_type,,}
    local nova_build_dir="$NOVA_BUILD_ROOT/$nova_build_name"

    cmake -S "$NOVA_SCRIPT_DIR" -B "$nova_build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE="$nova_build_type" \
        -DCMAKE_TOOLCHAIN_FILE="$NOVA_TOOLCHAIN_FILE" \
        -DVCPKG_TARGET_TRIPLET="$NOVA_TARGET_TRIPLET"
    cmake --build "$nova_build_dir" --parallel
}

case "${1:-}" in
    debug)
        build_config Debug
        ;;
    release)
        build_config Release
        ;;
    "")
        build_config Debug
        build_config Release
        ;;
    *)
        echo "usage: $0 [debug|release]" >&2
        exit 2
        ;;
esac
