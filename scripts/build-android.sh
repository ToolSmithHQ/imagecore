#!/usr/bin/env bash
set -euo pipefail

# Build imagecore and all dependencies for Android (arm64-v8a, armeabi-v7a, x86_64)
# Outputs: packages/native/android/prebuilt/{arm64-v8a,armeabi-v7a,x86_64}/libimagecore-all.a
#          packages/native/android/prebuilt/include/ (headers)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
CORE_DIR="$ROOT_DIR/packages/core"
OUTPUT_DIR="$ROOT_DIR/packages/native/android/prebuilt"

# Find Android NDK
ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
NDK_DIR=$(ls -d "$ANDROID_HOME/ndk/"* 2>/dev/null | sort -V | tail -1)
if [ -z "$NDK_DIR" ]; then
    echo "Error: Android NDK not found in $ANDROID_HOME/ndk/"
    exit 1
fi
echo "Using NDK: $NDK_DIR"

TOOLCHAIN="$NDK_DIR/build/cmake/android.toolchain.cmake"
MIN_API=24  # Android 7.0 — minimum for most RN apps
NPROC=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc)

mkdir -p "$OUTPUT_DIR/include"

build_abi() {
    local ABI=$1  # arm64-v8a, armeabi-v7a, x86_64

    local BUILD_DIR="$CORE_DIR/build-android-$ABI"
    local INSTALL_DIR="$BUILD_DIR/install"

    echo "==> Building for $ABI..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

    # Common CMake args for Android cross-compilation
    local ANDROID_ARGS=(
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
        -DANDROID_ABI="$ABI"
        -DANDROID_PLATFORM="android-$MIN_API"
        -DANDROID_STL=c++_shared
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
        -DBUILD_SHARED_LIBS=OFF
    )

    # ── zlib ──
    echo "  Building zlib..."
    cmake -S "$CORE_DIR/third_party/zlib" -B "$BUILD_DIR/zlib" \
        "${ANDROID_ARGS[@]}" \
        -DZLIB_BUILD_EXAMPLES=OFF \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/zlib" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/zlib" > /dev/null 2>&1

    # ── libjpeg-turbo ──
    echo "  Building libjpeg-turbo..."
    cmake -S "$CORE_DIR/third_party/libjpeg-turbo" -B "$BUILD_DIR/libjpeg-turbo" \
        "${ANDROID_ARGS[@]}" \
        -DENABLE_SHARED=OFF \
        -DWITH_TURBOJPEG=OFF \
        -DWITH_JAVA=OFF \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/libjpeg-turbo" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/libjpeg-turbo" > /dev/null 2>&1

    # ── libpng ──
    echo "  Building libpng..."
    cmake -S "$CORE_DIR/third_party/libpng" -B "$BUILD_DIR/libpng" \
        "${ANDROID_ARGS[@]}" \
        -DPNG_SHARED=OFF -DPNG_STATIC=ON \
        -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF \
        -DZLIB_LIBRARY="$INSTALL_DIR/lib/libz.a" \
        -DZLIB_INCLUDE_DIR="$INSTALL_DIR/include" \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/libpng" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/libpng" > /dev/null 2>&1

    # ── libwebp ──
    echo "  Building libwebp..."
    cmake -S "$CORE_DIR/third_party/libwebp" -B "$BUILD_DIR/libwebp" \
        "${ANDROID_ARGS[@]}" \
        -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF \
        -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF \
        -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF \
        -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF \
        -DWEBP_BUILD_ANIM_UTILS=OFF \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/libwebp" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/libwebp" > /dev/null 2>&1

    # ── libtiff ──
    echo "  Building libtiff..."
    cmake -S "$CORE_DIR/third_party/libtiff" -B "$BUILD_DIR/libtiff" \
        "${ANDROID_ARGS[@]}" \
        -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF \
        -Dtiff-docs=OFF -Dtiff-install=ON \
        -Dlzma=OFF -Djbig=OFF -Dwebp=OFF -Djpeg=OFF -Dzstd=OFF -Dlerc=OFF \
        -DZLIB_LIBRARY="$INSTALL_DIR/lib/libz.a" \
        -DZLIB_INCLUDE_DIR="$INSTALL_DIR/include" \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/libtiff" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/libtiff" > /dev/null 2>&1

    # ── imagecore (our code) ──
    echo "  Building imagecore..."

    # Get the NDK clang compiler path
    local CC=$(find "$NDK_DIR/toolchains/llvm/prebuilt" -name "clang" -not -name "clang++*" | head -1)
    local CXX=$(find "$NDK_DIR/toolchains/llvm/prebuilt" -name "clang++" | head -1)
    local AR=$(find "$NDK_DIR/toolchains/llvm/prebuilt" -name "llvm-ar" | head -1)
    local SYSROOT="$NDK_DIR/toolchains/llvm/prebuilt/darwin-x86_64/sysroot"

    # Map ABI to target triple
    local TARGET
    case "$ABI" in
        arm64-v8a)   TARGET="aarch64-linux-android$MIN_API" ;;
        armeabi-v7a) TARGET="armv7a-linux-androideabi$MIN_API" ;;
        x86_64)      TARGET="x86_64-linux-android$MIN_API" ;;
    esac

    local CFLAGS="--target=$TARGET --sysroot=$SYSROOT -O2 -fPIC"
    local CXXFLAGS="$CFLAGS -std=c++17"

    local IC_INCLUDES="-I$CORE_DIR/src -I$INSTALL_DIR/include -I$CORE_DIR/third_party/libjpeg-turbo/src -I$BUILD_DIR/libjpeg-turbo"

    local IC_SRCS=(
        "$CORE_DIR/src/imagecore.cpp"
        "$CORE_DIR/src/codecs/jpeg_codec.cpp"
        "$CORE_DIR/src/codecs/jpeg_lossless.cpp"
        "$CORE_DIR/src/codecs/png_codec.cpp"
        "$CORE_DIR/src/codecs/webp_codec.cpp"
        "$CORE_DIR/src/codecs/tiff_codec.cpp"
        "$CORE_DIR/src/codecs/bmp_codec.cpp"
        "$CORE_DIR/src/codecs/heif_codec.cpp"
        "$CORE_DIR/src/ops/exif.cpp"
        "$CORE_DIR/src/ops/resize.cpp"
        "$CORE_DIR/src/ops/crop.cpp"
        "$CORE_DIR/src/ops/rotate.cpp"
        "$CORE_DIR/src/ops/flip.cpp"
    )

    mkdir -p "$BUILD_DIR/imagecore-obj"
    for src in "${IC_SRCS[@]}"; do
        local obj="$BUILD_DIR/imagecore-obj/$(basename "${src%.*}").o"
        "$CXX" $CXXFLAGS $IC_INCLUDES -c "$src" -o "$obj" -Wall -Wno-unused-parameter 2>&1
    done

    # Compile C files
    "$CC" $CFLAGS $IC_INCLUDES -c "$CORE_DIR/third_party/libjpeg-turbo/src/transupp.c" \
        -o "$BUILD_DIR/imagecore-obj/transupp.o" 2>&1
    "$CC" $CFLAGS $IC_INCLUDES -c "$CORE_DIR/src/codecs/avif_codec_stub.c" \
        -o "$BUILD_DIR/imagecore-obj/avif_codec_stub.o" 2>&1

    # Create libimagecore.a
    "$AR" rcs "$INSTALL_DIR/lib/libimagecore.a" "$BUILD_DIR/imagecore-obj/"*.o

    # ── Create combined library ──
    echo "  Creating combined library..."
    mkdir -p "$OUTPUT_DIR/$ABI"

    # Must use NDK's llvm-ar (not macOS libtool) for Android ELF objects
    local MERGE_DIR="$BUILD_DIR/merge"
    rm -rf "$MERGE_DIR" && mkdir -p "$MERGE_DIR"
    cd "$MERGE_DIR"
    for lib in libimagecore libjpeg libpng libz libwebp libsharpyuv libtiff; do
        "$AR" x "$INSTALL_DIR/lib/$lib.a" 2>/dev/null || true
    done
    "$AR" rcs "$OUTPUT_DIR/$ABI/libimagecore-all.a" *.o
    cd "$CORE_DIR"

    echo "  Done: $OUTPUT_DIR/$ABI/libimagecore-all.a"
}

# Build for each ABI
build_abi "arm64-v8a"
build_abi "x86_64"

# Copy headers
cp "$CORE_DIR/src/imagecore.h" "$OUTPUT_DIR/include/"

echo ""
echo "==> Android build complete!"
for abi in arm64-v8a x86_64; do
    ls -lh "$OUTPUT_DIR/$abi/libimagecore-all.a" 2>/dev/null
done
echo ""
echo "Headers: $OUTPUT_DIR/include/"
