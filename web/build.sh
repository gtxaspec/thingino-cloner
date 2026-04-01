#!/bin/bash
# Build thingino-cloner for WebAssembly
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Source Emscripten environment
source "$HOME/emsdk/emsdk_env.sh" 2>/dev/null

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

emcmake cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc) VERBOSE=1

echo ""
echo "Build complete. Output in $SCRIPT_DIR/dist/"
ls -la "$SCRIPT_DIR/dist/"
