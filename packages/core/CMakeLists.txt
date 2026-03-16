cmake_minimum_required(VERSION 3.16)
project(imagecore C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(ExternalProject)

# ── Options ──────────────────────────────────────────────────────────── #

option(IC_BUILD_TESTS "Build tests" OFF)
option(IC_USE_LIBHEIF "Build with libheif (LGPL, WASM only)" OFF)

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
        -DCONFIG_AV1_HIGHBITDEPTH=0
        -DCONFIG_WEBM_IO=0
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
        -DAVIF_CODEC_AOM=SYSTEM
        -DAVIF_LIBYUV=OFF
        -DAVIF_BUILD_APPS=OFF
        -DAVIF_BUILD_TESTS=OFF
        -DAVIF_BUILD_EXAMPLES=OFF
        -DCMAKE_PREFIX_PATH=${IC_DEPS_INSTALL_DIR}
    DEPENDS libaom-ext
    BUILD_BYPRODUCTS ${IC_DEPS_INSTALL_DIR}/lib/libavif.a
)

add_library(avif-imported STATIC IMPORTED)
set_target_properties(avif-imported PROPERTIES
    IMPORTED_LOCATION ${IC_DEPS_INSTALL_DIR}/lib/libavif.a
)
add_dependencies(avif-imported libavif-ext)

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

# ── Compiler flags ───────────────────────────────────────────────────── #

target_compile_options(imagecore PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    $<$<CONFIG:Release>:-O2>
)

# ── Tests ────────────────────────────────────────────────────────────── #

if(IC_BUILD_TESTS)
    enable_testing()
    add_executable(test_imagecore tests/test_main.cpp)
    target_link_libraries(test_imagecore imagecore)
    add_test(NAME imagecore_tests COMMAND test_imagecore)
endif()
