#!/usr/bin/env bash
set -euo pipefail

# Build imagecore and all dependencies for iOS (device + simulator)
# Outputs: packages/native/ios/prebuilt/{device,simulator}/libimagecore-all.a
#          packages/native/ios/prebuilt/include/ (headers)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
CORE_DIR="$ROOT_DIR/packages/core"
OUTPUT_DIR="$ROOT_DIR/packages/native/ios/prebuilt"

IOS_SDK=$(xcrun --sdk iphoneos --show-sdk-path)
SIM_SDK=$(xcrun --sdk iphonesimulator --show-sdk-path)
MIN_IOS="15.0"

CC=$(xcrun --find clang)
CXX=$(xcrun --find clang++)
AR=$(xcrun --find ar)
RANLIB=$(xcrun --find ranlib)

NPROC=$(sysctl -n hw.logicalcpu)

mkdir -p "$OUTPUT_DIR"/{device,simulator,include}

build_target() {
    local TARGET=$1  # "device" or "simulator"
    local SDK=$2
    local ARCH="arm64"

    local BUILD_DIR="$CORE_DIR/build-ios-$TARGET"
    local INSTALL_DIR="$BUILD_DIR/install"

    echo "==> Building for $TARGET ($ARCH)..."
    rm -rf "$BUILD_DIR" "$INSTALL_DIR"
    mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

    local TARGET_TRIPLE
    if [ "$TARGET" = "simulator" ]; then
        TARGET_TRIPLE="arm64-apple-ios${MIN_IOS}-simulator"
    else
        TARGET_TRIPLE="arm64-apple-ios${MIN_IOS}"
    fi

    local CFLAGS="-arch $ARCH -isysroot $SDK -target $TARGET_TRIPLE -O2"
    local CXXFLAGS="$CFLAGS"
    local LDFLAGS="-arch $ARCH -isysroot $SDK -target $TARGET_TRIPLE"

    # ── zlib (using cmake for proper cross-compilation) ──
    echo "  Building zlib..."
    cmake -S "$CORE_DIR/third_party/zlib" -B "$BUILD_DIR/zlib" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_OSX_ARCHITECTURES=$ARCH \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN_IOS \
        -DCMAKE_OSX_SYSROOT="$SDK" \
        -DZLIB_BUILD_EXAMPLES=OFF \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/zlib" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/zlib" > /dev/null 2>&1

    # ── libjpeg-turbo ──
    echo "  Building libjpeg-turbo..."
    cmake -S "$CORE_DIR/third_party/libjpeg-turbo" -B "$BUILD_DIR/libjpeg-turbo" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_OSX_ARCHITECTURES=$ARCH \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN_IOS \
        -DCMAKE_OSX_SYSROOT="$SDK" \
        -DENABLE_SHARED=OFF \
        -DWITH_TURBOJPEG=OFF \
        -DWITH_JAVA=OFF \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/libjpeg-turbo" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/libjpeg-turbo" > /dev/null 2>&1

    # ── libpng ──
    echo "  Building libpng..."
    cmake -S "$CORE_DIR/third_party/libpng" -B "$BUILD_DIR/libpng" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_OSX_ARCHITECTURES=$ARCH \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN_IOS \
        -DCMAKE_OSX_SYSROOT="$SDK" \
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
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_OSX_ARCHITECTURES=$ARCH \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN_IOS \
        -DCMAKE_OSX_SYSROOT="$SDK" \
        -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF \
        -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF \
        -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF \
        -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF \
        -DWEBP_BUILD_ANIM_UTILS=OFF -DBUILD_SHARED_LIBS=OFF \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/libwebp" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/libwebp" > /dev/null 2>&1

    # ── libtiff ──
    echo "  Building libtiff..."
    cmake -S "$CORE_DIR/third_party/libtiff" -B "$BUILD_DIR/libtiff" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_OSX_ARCHITECTURES=$ARCH \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN_IOS \
        -DCMAKE_OSX_SYSROOT="$SDK" \
        -DBUILD_SHARED_LIBS=OFF \
        -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF \
        -Dtiff-docs=OFF -Dtiff-install=ON \
        -Dlzma=OFF -Djbig=OFF -Dwebp=OFF -Djpeg=OFF -Dzstd=OFF -Dlerc=OFF \
        -DZLIB_LIBRARY="$INSTALL_DIR/lib/libz.a" \
        -DZLIB_INCLUDE_DIR="$INSTALL_DIR/include" \
        > /dev/null 2>&1
    cmake --build "$BUILD_DIR/libtiff" -j$NPROC > /dev/null 2>&1
    cmake --install "$BUILD_DIR/libtiff" > /dev/null 2>&1

    # NOTE: libaom + libavif skipped for iOS — AVIF decode/encode handled by
    # platform APIs on iOS 16+, and HEIC (the primary iPhone format) uses
    # platform APIs via PlatformHeicEncoder.mm. AVIF support will be added later.

    # ── imagecore (our code) ──
    echo "  Building imagecore..."
    # Compile our source files directly with clang (no cmake for this step)
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

    local IC_INCLUDES="-I$CORE_DIR/src -I$INSTALL_DIR/include -I$CORE_DIR/third_party/libjpeg-turbo/src -I$BUILD_DIR/libjpeg-turbo"

    mkdir -p "$BUILD_DIR/imagecore-obj"
    for src in "${IC_SRCS[@]}"; do
        local obj="$BUILD_DIR/imagecore-obj/$(basename "$src" .cpp).o"
        obj="${obj%.c.o}"  # handle .c files too
        "$CXX" -std=c++17 $CXXFLAGS $IC_INCLUDES -c "$src" -o "${obj}.o" -Wall -Wno-unused-parameter 2>&1
    done

    # Also compile transupp.c and AVIF stub
    "$CC" $CFLAGS $IC_INCLUDES -c "$CORE_DIR/third_party/libjpeg-turbo/src/transupp.c" \
        -o "$BUILD_DIR/imagecore-obj/transupp.o" 2>&1
    "$CC" $CFLAGS $IC_INCLUDES -c "$CORE_DIR/src/codecs/avif_codec_stub.c" \
        -o "$BUILD_DIR/imagecore-obj/avif_codec_stub.o" 2>&1

    # Create libimagecore.a
    $AR rcs "$INSTALL_DIR/lib/libimagecore.a" "$BUILD_DIR/imagecore-obj/"*.o

    # ── Create combined fat library ──
    echo "  Creating combined library..."
    libtool -static -o "$OUTPUT_DIR/$TARGET/libimagecore-all.a" \
        "$INSTALL_DIR/lib/libimagecore.a" \
        "$INSTALL_DIR/lib/libjpeg.a" \
        "$INSTALL_DIR/lib/libpng.a" \
        "$INSTALL_DIR/lib/libz.a" \
        "$INSTALL_DIR/lib/libwebp.a" \
        "$INSTALL_DIR/lib/libsharpyuv.a" \
        "$INSTALL_DIR/lib/libtiff.a" \
        2>&1

    echo "  ✓ $TARGET done: $OUTPUT_DIR/$TARGET/libimagecore-all.a"
}

# Build for both targets
build_target "device" "$IOS_SDK"
build_target "simulator" "$SIM_SDK"

# Copy headers
cp "$CORE_DIR/src/imagecore.h" "$OUTPUT_DIR/include/"
cp -r "$CORE_DIR/build-ios-device/install/include/avif" "$OUTPUT_DIR/include/" 2>/dev/null || true

echo ""
echo "==> Build complete!"
ls -lh "$OUTPUT_DIR"/device/libimagecore-all.a "$OUTPUT_DIR"/simulator/libimagecore-all.a
echo ""
echo "Headers: $OUTPUT_DIR/include/"
