/**
 * @toolsmith/imagecore-native — Android JNI + JSI bridge
 *
 * Loads the native C++ library and installs the JSI host object.
 * Also initializes the platform HEIC encoder JNI bridge.
 */

#include <jni.h>
#include <jsi/jsi.h>

#include "ImageCoreHostObject.h"
#include "PlatformHeicBridge.h"

extern "C"
JNIEXPORT void JNICALL
Java_com_toolsmith_imagecore_ImageCoreModule_nativeInstall(
    JNIEnv* env,
    jobject thiz,
    jlong jsiRuntimePtr) {

    // Initialize platform HEIC bridge (JNI → PlatformHeicEncoder.kt)
    platform_heic_set_jni(env, nullptr);

    // Install JSI host object
    auto& runtime = *reinterpret_cast<facebook::jsi::Runtime*>(jsiRuntimePtr);
    imagecore::install(runtime);
}
