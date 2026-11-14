#!/usr/bin/env bash
# Build imagecore.js + imagecore.wasm with Emscripten.
#
# Usage: bash packages/web/scripts/build-wasm.sh [OUT_DIR]
#   OUT_DIR defaults to packages/web/src. The CMake build tree lives in
#   packages/core/build-wasm (git-ignored); delete it for a clean rebuild.
#
# Env overrides:
#   IC_WASM_BUILD_DIR   CMake build dir (default packages/core/build-wasm)
#   IC_USE_LIBHEIF      ON/OFF (default ON) - HEIC decode (libde265) + encode (kvazaar)
#   JOBS                parallel build jobs
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORE_DIR="$(cd "$SCRIPT_DIR/../../core" && pwd)"
BUILD_DIR="${IC_WASM_BUILD_DIR:-$CORE_DIR/build-wasm}"
OUT_DIR="${1:-$SCRIPT_DIR/../src}"
JOBS="${JOBS:-$(sysctl -n hw.logicalcpu 2>/dev/null || nproc)}"

if ! command -v emcmake &> /dev/null; then
    if [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
        # shellcheck disable=SC1091
        source "$HOME/emsdk/emsdk_env.sh" > /dev/null 2>&1
    fi
fi
if ! command -v emcmake &> /dev/null; then
    echo "Error: Emscripten not found. Install it from https://emscripten.org/docs/getting_started/"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

if [ ! -f "$CORE_DIR/third_party/libaom/CMakeLists.txt" ]; then
    echo "==> Initializing submodules..."
    git -C "$CORE_DIR" submodule update --init --recursive
fi

echo "==> Configuring ($BUILD_DIR)..."
emcmake cmake -S "$CORE_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DIC_USE_LIBHEIF="${IC_USE_LIBHEIF:-ON}"

echo "==> Building with $JOBS jobs..."
cmake --build "$BUILD_DIR" --config Release --target imagecore_wasm -j"$JOBS"

mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/imagecore.js" "$BUILD_DIR/imagecore.wasm" "$OUT_DIR/"
echo "==> Done: $OUT_DIR/imagecore.{js,wasm} ($(du -h "$OUT_DIR/imagecore.wasm" | cut -f1) wasm)"
