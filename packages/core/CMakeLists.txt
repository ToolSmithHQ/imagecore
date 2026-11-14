cmake_minimum_required(VERSION 3.16)
project(imagecore C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(ExternalProject)

# ── Options ──────────────────────────────────────────────────────────── #

option(IC_BUILD_TESTS "Build tests" OFF)
option(IC_USE_LIBHEIF "Build with libheif (LGPL, WASM only)" OFF)
# libaom's AV1 encoder (recursive partition search) overflows Emscripten's
# default 64 KB stack; libde265/kvazaar are stack-hungry too.
set(IC_WASM_STACK_SIZE "1048576" CACHE STRING "WASM stack size in bytes")
set(IC_WASM_LINK_OPT "-O3" CACHE STRING "Link-time (wasm-opt) optimization level for the WASM module")
set(IC_WASM_EXTRA_LINK_FLAGS "" CACHE STRING "Extra emcc link flags for the WASM module")

set(IC_DEPS_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps)

# ── Cross-compilation flags (forwarded to all ExternalProject builds) ── #

set(IC_CROSS_COMPILE_ARGS)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND IC_CROSS_COMPILE_ARGS
        -DCMAKE_SYSTEM_NAME=iOS
        -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
    )
elseif(EMSCRIPTEN)
    list(APPEND IC_CROSS_COMPILE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_CROSSCOMPILING_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}
        # WASM has no dynamic linking — all deps must be static
        -DBUILD_SHARED_LIBS=OFF
    )
    # libaom needs explicit generic CPU target for WASM (no x86/ARM SIMD)
    set(IC_LIBAOM_WASM_ARGS -DAOM_TARGET_CPU=generic)
endif()

# ── libjpeg-turbo (ExternalProject — it blocks add_subdirectory) ────── #

ExternalProject_Add(libjpeg-turbo-ext
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg-turbo
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${IC_CROSS_COMPILE_ARGS}
        -DENABLE_SHARED=OFF
        -DWITH_TURBOJPEG=OFF
        -DWITH_JAVA=OFF
    BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libjpeg.a
)

add_library(jpeg-imported STATIC IMPORTED)
set_target_properties(jpeg-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libjpeg.a
)
add_dependencies(jpeg-imported libjpeg-turbo-ext)

# ── zlib (ExternalProject for consistency) ───────────────────────────── #

ExternalProject_Add(zlib-ext
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${IC_CROSS_COMPILE_ARGS}
        -DZLIB_BUILD_EXAMPLES=OFF
        -DZLIB_BUILD_TESTING=OFF
        -DZLIB_BUILD_SHARED=OFF
    BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libz.a
)

add_library(zlib-imported STATIC IMPORTED)
set_target_properties(zlib-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libz.a
)
add_dependencies(zlib-imported zlib-ext)

# ── libpng (ExternalProject — needs zlib) ────────────────────────────── #

ExternalProject_Add(libpng-ext
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libpng
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${IC_CROSS_COMPILE_ARGS}
        -DPNG_SHARED=OFF
        -DPNG_STATIC=ON
        -DPNG_TESTS=OFF
        -DPNG_EXECUTABLES=OFF
        -DZLIB_LIBRARY=${IC_DEPS_INSTALL_DIR}/lib/libz.a
        -DZLIB_INCLUDE_DIR=${IC_DEPS_INSTALL_DIR}/include
    DEPENDS zlib-ext
    BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libpng.a
                     ${IC_DEPS_INSTALL_DIR}/lib/libpng.a
)

add_library(png-imported STATIC IMPORTED)
set_target_properties(png-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libpng.a
)
add_dependencies(png-imported libpng-ext)

# ── libwebp (ExternalProject) ────────────────────────────────────────── #

ExternalProject_Add(libwebp-ext
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libwebp
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${IC_CROSS_COMPILE_ARGS}
        -DWEBP_BUILD_CWEBP=OFF
        -DWEBP_BUILD_DWEBP=OFF
        -DWEBP_BUILD_GIF2WEBP=OFF
        -DWEBP_BUILD_IMG2WEBP=OFF
        -DWEBP_BUILD_VWEBP=OFF
        -DWEBP_BUILD_WEBPINFO=OFF
        -DWEBP_BUILD_WEBPMUX=OFF
        -DWEBP_BUILD_EXTRAS=OFF
        -DWEBP_BUILD_ANIM_UTILS=OFF
        -DBUILD_SHARED_LIBS=OFF
    BUILD_BYPRODUCTS
        ${IC_DEPS_INSTALL_DIR}/lib/libwebp.a
        ${IC_DEPS_INSTALL_DIR}/lib/libsharpyuv.a
)

add_library(webp-imported STATIC IMPORTED)
set_target_properties(webp-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libwebp.a
)
add_dependencies(webp-imported libwebp-ext)

add_library(sharpyuv-imported STATIC IMPORTED)
set_target_properties(sharpyuv-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libsharpyuv.a
)
add_dependencies(sharpyuv-imported libwebp-ext)

# ── libtiff (ExternalProject) ────────────────────────────────────────── #

ExternalProject_Add(libtiff-ext
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtiff
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${IC_CROSS_COMPILE_ARGS}
        -DBUILD_SHARED_LIBS=OFF
        -Dtiff-tools=OFF
        -Dtiff-tests=OFF
        -Dtiff-contrib=OFF
        -Dtiff-docs=OFF
        -Dtiff-install=ON
        -Dlzma=OFF
        -Djbig=OFF
        -Dwebp=OFF
        -Djpeg=OFF
        -Dzstd=OFF
        -Dlerc=OFF
        -DZLIB_LIBRARY=${IC_DEPS_INSTALL_DIR}/lib/libz.a
        -DZLIB_INCLUDE_DIR=${IC_DEPS_INSTALL_DIR}/include
    DEPENDS zlib-ext
    BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libtiff.a
)

add_library(tiff-imported STATIC IMPORTED)
set_target_properties(tiff-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libtiff.a
)
add_dependencies(tiff-imported libtiff-ext)

# ── libaom (ExternalProject — large, separate build) ─────────────────── #

ExternalProject_Add(libaom-ext
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libaom
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${IC_CROSS_COMPILE_ARGS}
        -DENABLE_DOCS=OFF
        -DENABLE_EXAMPLES=OFF
        -DENABLE_TESTDATA=OFF
        -DENABLE_TESTS=OFF
        -DENABLE_TOOLS=OFF
        # 10/12-bit AVIF (HDR, most phone cameras) needs high bitdepth support
        -DCONFIG_AV1_HIGHBITDEPTH=1
        -DCONFIG_WEBM_IO=0
        ${IC_LIBAOM_WASM_ARGS}
    BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libaom.a
)

add_library(aom-imported STATIC IMPORTED)
set_target_properties(aom-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libaom.a
)
add_dependencies(aom-imported libaom-ext)

# ── libavif (ExternalProject — uses libaom for AV1, handles ISO BMFF) ── #

ExternalProject_Add(libavif-ext
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libavif
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${IC_CROSS_COMPILE_ARGS}
        -DBUILD_SHARED_LIBS=OFF
        -DAVIF_BUILD_SHARED_LIBS=OFF
        -DAVIF_CODEC_AOM=SYSTEM
        -DAVIF_LIBYUV=OFF
        -DAVIF_BUILD_APPS=OFF
        -DAVIF_BUILD_TESTS=OFF
        -DAVIF_BUILD_EXAMPLES=OFF
        -DCMAKE_PREFIX_PATH=${IC_DEPS_INSTALL_DIR}
        -DCMAKE_FIND_ROOT_PATH=${IC_DEPS_INSTALL_DIR}
    DEPENDS libaom-ext
    BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libavif.a
)

add_library(avif-imported STATIC IMPORTED)
set_target_properties(avif-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libavif.a
)
add_dependencies(avif-imported libavif-ext)

# ── HEIC: libde265 (decode) + kvazaar (encode) + libheif ─────────────── #
#
# Off by default: iOS/Android use platform HEIC APIs. The WASM build
# (packages/web/scripts/build-wasm.sh) turns it on. Licenses: libheif and
# libde265 are LGPL-3.0, kvazaar is BSD-3-Clause. x265 (GPL) is never used.
# Built single-threaded (no pthreads in the WASM module).

if(IC_USE_LIBHEIF)
    set(IC_LIBHEIF_EXTRA_ARGS)
    # libheif is container parsing + color conversion, not hot codec loops:
    # optimize it for size (saves ~0.5 MB of WASM at negligible speed cost).
    if(EMSCRIPTEN)
        set(IC_LIBHEIF_BUILD_TYPE MinSizeRel)
    else()
        set(IC_LIBHEIF_BUILD_TYPE ${CMAKE_BUILD_TYPE})
    endif()
    if(EMSCRIPTEN)
        # Skip libheif's built-in embind JS bindings (heif_emscripten.h); we
        # export our own C API. Same switch libheif's build-emscripten.sh uses.
        list(APPEND IC_LIBHEIF_EXTRA_ARGS
            "-DCMAKE_CXX_FLAGS=-D__EMSCRIPTEN_STANDALONE_WASM__=1")
    endif()

    ExternalProject_Add(libde265-ext
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libde265
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            ${IC_CROSS_COMPILE_ARGS}
            -DBUILD_SHARED_LIBS=OFF
            -DENABLE_SDL=OFF
            -DENABLE_DECODER=OFF
            -DENABLE_ENCODER=OFF
            -DENABLE_TOOLS=OFF
        BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libde265.a
    )

    ExternalProject_Add(kvazaar-ext
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kvazaar
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
            -DCMAKE_INSTALL_LIBDIR=lib
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            ${IC_CROSS_COMPILE_ARGS}
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTS=OFF
            -DBUILD_KVAZAAR_BINARY=OFF
            -DGIT_SUBMODULE=OFF
            -DUSE_CRYPTO=OFF
        BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libkvazaar.a
    )

    # libheif is built from a patched copy so the submodule stays pristine
    # (see patches/libheif-keep-deblocking.patch).
    ExternalProject_Add(libheif-ext
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/libheif-src
        DOWNLOAD_COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libheif
            ${CMAKE_CURRENT_BINARY_DIR}/libheif-src
        # Idempotent: skip if already applied (a re-run of only the patch step would
        # otherwise fail with "Reversed (or previously applied) patch detected").
        PATCH_COMMAND sh -c "patch -p1 -R -s -f --dry-run -i '${CMAKE_CURRENT_SOURCE_DIR}/patches/libheif-keep-deblocking.patch' >/dev/null 2>&1 || patch -p1 -N -i '${CMAKE_CURRENT_SOURCE_DIR}/patches/libheif-keep-deblocking.patch'"
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${IC_DEPS_INSTALL_DIR}
            -DCMAKE_INSTALL_LIBDIR=lib
            -DCMAKE_BUILD_TYPE=${IC_LIBHEIF_BUILD_TYPE}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            ${IC_CROSS_COMPILE_ARGS}
            -DBUILD_SHARED_LIBS=OFF
            -DENABLE_PLUGIN_LOADING=OFF
            -DENABLE_MULTITHREADING_SUPPORT=OFF
            -DENABLE_PARALLEL_TILE_DECODING=OFF
            -DWITH_LIBDE265=ON
            -DWITH_LIBDE265_PLUGIN=OFF
            -DLIBDE265_INCLUDE_DIR=${IC_DEPS_INSTALL_DIR}/include
            -DLIBDE265_LIBRARY=${IC_DEPS_INSTALL_DIR}/lib/libde265.a
            -DWITH_KVAZAAR=ON
            -DWITH_KVAZAAR_PLUGIN=OFF
            -DKVAZAAR_INCLUDE_DIR=${IC_DEPS_INSTALL_DIR}/include
            -DKVAZAAR_LIBRARY=${IC_DEPS_INSTALL_DIR}/lib/libkvazaar.a
            -DWITH_X265=OFF
            -DWITH_X264=OFF
            -DWITH_OpenH264_DECODER=OFF
            -DWITH_AOM_DECODER=OFF
            -DWITH_AOM_ENCODER=OFF
            -DWITH_DAV1D=OFF
            -DWITH_RAV1E=OFF
            -DWITH_SvtEnc=OFF
            -DWITH_JPEG_DECODER=OFF
            -DWITH_JPEG_ENCODER=OFF
            -DWITH_OpenJPEG_DECODER=OFF
            -DWITH_OpenJPEG_ENCODER=OFF
            -DWITH_OPENJPH_ENCODER=OFF
            -DWITH_FFMPEG_DECODER=OFF
            -DWITH_UVG266=OFF
            -DWITH_VVDEC=OFF
            -DWITH_VVENC=OFF
            -DWITH_UNCOMPRESSED_CODEC=OFF
            -DWITH_WEBCODECS=OFF
            -DWITH_LIBSHARPYUV=OFF
            -DWITH_HEADER_COMPRESSION=OFF
            -DHEIF_WITH_OMAF=OFF
            -DWITH_EXAMPLES=OFF
            -DWITH_GDK_PIXBUF=OFF
            -DBUILD_DOCUMENTATION=OFF
            -DBUILD_TESTING=OFF
            -DWITH_FUZZERS=OFF
            ${IC_LIBHEIF_EXTRA_ARGS}
            -DCMAKE_PREFIX_PATH=${IC_DEPS_INSTALL_DIR}
            -DCMAKE_FIND_ROOT_PATH=${IC_DEPS_INSTALL_DIR}
        DEPENDS libde265-ext kvazaar-ext
        BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libheif.a
    )

    foreach(_lib heif de265 kvazaar)
        add_library(${_lib}-imported STATIC IMPORTED)
        set_target_properties(${_lib}-imported PROPERTIES
            IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/lib${_lib}.a)
    endforeach()
    add_dependencies(heif-imported libheif-ext)
    add_dependencies(de265-imported libde265-ext)
    add_dependencies(kvazaar-imported kvazaar-ext)
endif()

# ── imagecore static library ─────────────────────────────────────────── #

set(IC_SOURCES
    src/imagecore.cpp
    src/codecs/jpeg_codec.cpp
    src/codecs/jpeg_lossless.cpp
    # transupp.c must be compiled into our project (not part of libjpeg lib)
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg-turbo/src/transupp.c
    src/codecs/png_codec.cpp
    src/codecs/webp_codec.cpp
    src/codecs/tiff_codec.cpp
    src/codecs/bmp_codec.cpp
    src/codecs/avif_codec.cpp
    src/codecs/heif_codec.cpp
    src/ops/exif.cpp
    src/ops/resize.cpp
    src/ops/crop.cpp
    src/ops/rotate.cpp
    src/ops/flip.cpp
)

add_library(imagecore STATIC ${IC_SOURCES})

# Include dirs from installed deps
target_include_directories(imagecore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IC_DEPS_INSTALL_DIR}/include
    # transupp.h lives in the libjpeg-turbo source tree (not installed)
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg-turbo/src
    # jconfigint.h is generated by libjpeg-turbo's cmake build
    ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-ext-prefix/src/libjpeg-turbo-ext-build
)

# Ensure deps are built before imagecore
add_dependencies(imagecore
    libjpeg-turbo-ext
    zlib-ext
    libpng-ext
    libwebp-ext
    libtiff-ext
    libaom-ext
    libavif-ext
)

target_link_libraries(imagecore PUBLIC
    jpeg-imported
    png-imported
    zlib-imported
    webp-imported
    sharpyuv-imported
    tiff-imported
    avif-imported
    aom-imported
)

if(IC_USE_LIBHEIF)
    add_dependencies(imagecore libheif-ext)
    target_compile_definitions(imagecore PRIVATE IC_HAS_LIBHEIF LIBHEIF_STATIC_BUILD)
    # Order matters for static linking: libheif before its codecs.
    target_link_libraries(imagecore PUBLIC heif-imported de265-imported kvazaar-imported)
    if(NOT EMSCRIPTEN)
        find_package(Threads REQUIRED)
        target_link_libraries(imagecore PUBLIC Threads::Threads)
    endif()
endif()

# ── Compiler flags ───────────────────────────────────────────────────── #

target_compile_options(imagecore PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    $<$<CONFIG:Release>:-O2>
)

# ── WebAssembly module (Emscripten only) ─────────────────────────────── #
#
# Produces imagecore.js + imagecore.wasm (MODULARIZE, factory createImageCore).
# The exported C ABI is consumed by packages/web and the imagesmith-web app;
# keep the export list stable.

if(EMSCRIPTEN)
    set(IC_WASM_EXPORTS
        _ic_detect_format _ic_get_image_info _ic_decode _ic_encode _ic_convert
        _ic_jpeg_lossless_rotate _ic_jpeg_lossless_crop _ic_jpeg_strip_exif
        _ic_strip_exif _ic_resize _ic_crop _ic_rotate
        _ic_flip_horizontal _ic_flip_vertical
        _ic_get_exif _ic_read_exif_internal
        _ic_free _ic_image_free _ic_version
        _malloc _free
    )
    list(JOIN IC_WASM_EXPORTS "," IC_WASM_EXPORTS_STR)

    add_executable(imagecore_wasm src/wasm_main.c)
    set_target_properties(imagecore_wasm PROPERTIES OUTPUT_NAME imagecore SUFFIX ".js")
    target_link_libraries(imagecore_wasm PRIVATE imagecore)
    target_link_options(imagecore_wasm PRIVATE
        ${IC_WASM_LINK_OPT}
        -sMODULARIZE=1
        -sEXPORT_NAME=createImageCore
        "-sEXPORTED_FUNCTIONS=${IC_WASM_EXPORTS_STR}"
        "-sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU32,HEAPF32,UTF8ToString,ccall,cwrap"
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=32MB
        -sMAXIMUM_MEMORY=2GB
        -sSTACK_SIZE=${IC_WASM_STACK_SIZE}
        -sFILESYSTEM=0
        ${IC_WASM_EXTRA_LINK_FLAGS}
    )
endif()

# ── Tests ────────────────────────────────────────────────────────────── #

if(IC_BUILD_TESTS)
    enable_testing()
    add_executable(test_imagecore
        tests/test_main.cpp
        tests/test_format_detection.cpp
        tests/test_codec_roundtrips.cpp
        tests/test_lossless_jpeg.cpp
        tests/test_pixel_ops.cpp
        tests/test_sample_images.cpp
        tests/test_exif.cpp
    )
    target_include_directories(test_imagecore PRIVATE tests)
    target_link_libraries(test_imagecore imagecore)
    add_test(NAME imagecore_tests COMMAND test_imagecore)
endif()
