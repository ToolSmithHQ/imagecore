/**
 * @toolsmith/imagecore-native — Android JNI + JSI bridge
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
    jlong jsiRuntimePtr,
    jobject context) {

    // Initialize platform HEIC bridge with app context (for classloader)
    platform_heic_set_jni(env, context);

    // Install JSI host object
    auto& runtime = *reinterpret_cast<facebook::jsi::Runtime*>(jsiRuntimePtr);
    imagecore::install(runtime);
}
