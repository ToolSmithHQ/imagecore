#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"
OUT_DIR="$SCRIPT_DIR/../src"

echo "==> Building imagecore WASM..."

# Check for Emscripten
if ! command -v emcmake &> /dev/null; then
    echo "Error: Emscripten not found. Install it from https://emscripten.org/docs/getting_started/"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

# Configure
echo "==> Configuring with emcmake..."
emcmake cmake -S "$BUILD_DIR" -B "$BUILD_DIR/build_wasm" \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo "==> Building with emmake..."
emmake cmake --build "$BUILD_DIR/build_wasm" --config Release -j$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)

# Copy outputs
echo "==> Copying WASM artifacts..."
cp "$BUILD_DIR/build_wasm/imagecore.js" "$OUT_DIR/"
cp "$BUILD_DIR/build_wasm/imagecore.wasm" "$OUT_DIR/"

echo "==> Done! Output: $OUT_DIR/imagecore.{js,wasm}"
