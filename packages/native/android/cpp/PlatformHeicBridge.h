#ifndef IC_PLATFORM_HEIC_BRIDGE_ANDROID_H
#define IC_PLATFORM_HEIC_BRIDGE_ANDROID_H

#include <jni.h>
#include "imagecore.h"

/**
 * Android HEIC bridge — calls PlatformHeicEncoder.kt via JNI.
 *
 * The JNIEnv* must be set before use via platform_heic_set_jni().
 * This is called once from ImageCoreModule.nativeInstall().
 */

#ifdef __cplusplus
extern "C" {
#endif

void platform_heic_set_jni(JNIEnv* env, jobject classLoader);

int platform_heic_decode(const uint8_t* data, size_t len, ICImage* out);
int platform_heic_encode(const ICImage* img, const ICEncodeOpts* opts,
                         uint8_t** out_data, size_t* out_len);
int platform_heic_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

#ifdef __cplusplus
}
#endif

#endif
