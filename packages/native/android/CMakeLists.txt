cmake_minimum_required(VERSION 3.16)
project(imagecore-android CXX)

set(CMAKE_CXX_STANDARD 17)

# ── Prebuilt imagecore library (per ABI) ─────────────────────────────── #

add_library(imagecore-prebuilt STATIC IMPORTED)
set_target_properties(imagecore-prebuilt PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/${ANDROID_ABI}/libimagecore-all.a
)

# ── React Native (prefab packages) ───────────────────────────────────── #

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

# ── JNI + JSI bridge ────────────────────────────────────────────────── #

add_library(imagecore-jni SHARED
    cpp/ImageCoreJSI.cpp
    cpp/PlatformHeicBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ios/ImageCoreHostObject.cpp
)

target_include_directories(imagecore-jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ios
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/include
)

# RN 0.76+ merged reactnativejni into reactnative
target_link_libraries(imagecore-jni
    imagecore-prebuilt
    ReactAndroid::jsi
    ReactAndroid::reactnative
    fbjni::fbjni
    android
    log
)
