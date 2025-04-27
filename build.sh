#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR="$SCRIPT_DIR/build"

mkdir -p "$BUILD_DIR"

build_debug() {
    echo "Building DEBUG version..."
    mkdir -p "$BUILD_DIR/debug"
    cd "$BUILD_DIR/debug" || exit 1
    cmake -DCMAKE_BUILD_TYPE=Debug "$SCRIPT_DIR"
    cmake --build .
}

build_release() {
    echo "Building RELEASE version..."
    mkdir -p "$BUILD_DIR/release"
    cd "$BUILD_DIR/release" || exit 1
    cmake -DCMAKE_BUILD_TYPE=Release "$SCRIPT_DIR"
    cmake --build .
}

case "$1" in
    debug)
        build_debug
        ;;
    release)
        build_release
        ;;
    *)
        # 无参数时同时构建debug和release
        build_debug
        build_release
        ;;
esac

echo "Build completed!"
