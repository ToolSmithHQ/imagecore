/**
 * JNI bridge to PlatformHeicEncoder.kt for HEIC encode/decode on Android.
 *
 * Converts between C uint8_t* RGBA buffers and Java int[] ARGB_8888 pixels.
 */

#include "PlatformHeicBridge.h"

#include <cstdlib>
#include <cstring>

static JavaVM* g_jvm = nullptr;
static jclass g_heicClass = nullptr;
static jmethodID g_decodeMethod = nullptr;
static jmethodID g_encodeMethod = nullptr;
static jmethodID g_getInfoMethod = nullptr;

static JNIEnv* get_env() {
    JNIEnv* env = nullptr;
    if (!g_jvm) return nullptr;

    int status = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return nullptr;
        }
    }
    return env;
}

extern "C" void platform_heic_set_jni(JNIEnv* env, jobject context) {
    env->GetJavaVM(&g_jvm);

    // Use the app classloader to find our class (FindClass fails on non-main threads)
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getClassLoader = env->GetMethodID(contextClass, "getClassLoader",
                                                 "()Ljava/lang/ClassLoader;");

    jclass cls = nullptr;

    if (getClassLoader) {
        // Try via app classloader
        jobject classLoader = env->CallObjectMethod(context, getClassLoader);
        if (classLoader) {
            jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
            jmethodID loadClass = env->GetMethodID(classLoaderClass, "loadClass",
                                                    "(Ljava/lang/String;)Ljava/lang/Class;");
            jstring className = env->NewStringUTF("com.toolsmith.imagecore.PlatformHeicEncoder");
            cls = static_cast<jclass>(env->CallObjectMethod(classLoader, loadClass, className));
            env->DeleteLocalRef(className);
            env->DeleteLocalRef(classLoader);
            env->DeleteLocalRef(classLoaderClass);

            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                cls = nullptr;
            }
        }
    }

    // Fallback to FindClass (works on main thread)
    if (!cls) {
        cls = env->FindClass("com/toolsmith/imagecore/PlatformHeicEncoder");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
        }
    }

    if (!cls) return;

    g_heicClass = static_cast<jclass>(env->NewGlobalRef(cls));
    g_decodeMethod = env->GetStaticMethodID(g_heicClass, "decode", "([B)[I");
    g_encodeMethod = env->GetStaticMethodID(g_heicClass, "encode", "([IIII)[B");
    g_getInfoMethod = env->GetStaticMethodID(g_heicClass, "getInfo", "([B)[I");

    env->DeleteLocalRef(contextClass);
}

/* ── ARGB <-> RGBA swizzle ───────────────────────────────────────────── */

static void argb_to_rgba(const int32_t* argb, uint8_t* rgba, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; i++) {
        uint32_t p = static_cast<uint32_t>(argb[i]);
        rgba[i * 4 + 0] = (p >> 16) & 0xFF;
        rgba[i * 4 + 1] = (p >> 8)  & 0xFF;
        rgba[i * 4 + 2] =  p        & 0xFF;
        rgba[i * 4 + 3] = (p >> 24) & 0xFF;
    }
}

static void rgba_to_argb(const uint8_t* rgba, int32_t* argb, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];
        argb[i] = static_cast<int32_t>((a << 24) | (r << 16) | (g << 8) | b);
    }
}

/* ── Decode ──────────────────────────────────────────────────────────── */

extern "C" int platform_heic_decode(const uint8_t* data, size_t len, ICImage* out) {
    JNIEnv* env = get_env();
    if (!env || !g_heicClass || !g_decodeMethod)
        return IC_ERROR_UNSUPPORTED_FORMAT;

    jbyteArray jdata = env->NewByteArray(static_cast<jsize>(len));
    if (!jdata) return IC_ERROR_ALLOC_FAILED;
    env->SetByteArrayRegion(jdata, 0, static_cast<jsize>(len),
                            reinterpret_cast<const jbyte*>(data));

    jintArray result = static_cast<jintArray>(
        env->CallStaticObjectMethod(g_heicClass, g_decodeMethod, jdata));
    env->DeleteLocalRef(jdata);

    if (!result || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return IC_ERROR_DECODE_FAILED;
    }

    /* Get dimensions via getInfo */
    jbyteArray jdata2 = env->NewByteArray(static_cast<jsize>(len));
    env->SetByteArrayRegion(jdata2, 0, static_cast<jsize>(len),
                            reinterpret_cast<const jbyte*>(data));
    jintArray info = static_cast<jintArray>(
        env->CallStaticObjectMethod(g_heicClass, g_getInfoMethod, jdata2));
    env->DeleteLocalRef(jdata2);

    if (!info) {
        env->DeleteLocalRef(result);
        return IC_ERROR_DECODE_FAILED;
    }

    jint* infoData = env->GetIntArrayElements(info, nullptr);
    out->width = static_cast<uint32_t>(infoData[0]);
    out->height = static_cast<uint32_t>(infoData[1]);
    env->ReleaseIntArrayElements(info, infoData, 0);
    env->DeleteLocalRef(info);

    out->stride = out->width * 4;
    size_t pixel_count = static_cast<size_t>(out->width) * out->height;
    out->pixels = static_cast<uint8_t*>(malloc(pixel_count * 4));
    if (!out->pixels) {
        env->DeleteLocalRef(result);
        return IC_ERROR_ALLOC_FAILED;
    }

    jint* pixels = env->GetIntArrayElements(result, nullptr);
    argb_to_rgba(pixels, out->pixels, pixel_count);
    env->ReleaseIntArrayElements(result, pixels, 0);
    env->DeleteLocalRef(result);

    return IC_OK;
}

/* ── Encode ──────────────────────────────────────────────────────────── */

extern "C" int platform_heic_encode(const ICImage* img, const ICEncodeOpts* opts,
                                     uint8_t** out_data, size_t* out_len) {
    JNIEnv* env = get_env();
    if (!env || !g_heicClass || !g_encodeMethod)
        return IC_ERROR_UNSUPPORTED_FORMAT;

    size_t pixel_count = static_cast<size_t>(img->width) * img->height;

    jintArray jpixels = env->NewIntArray(static_cast<jsize>(pixel_count));
    if (!jpixels) return IC_ERROR_ALLOC_FAILED;

    int32_t* argb = static_cast<int32_t*>(malloc(pixel_count * sizeof(int32_t)));
    if (!argb) {
        env->DeleteLocalRef(jpixels);
        return IC_ERROR_ALLOC_FAILED;
    }

    rgba_to_argb(img->pixels, argb, pixel_count);
    env->SetIntArrayRegion(jpixels, 0, static_cast<jsize>(pixel_count), argb);
    free(argb);

    int quality = static_cast<int>((opts ? opts->quality : 0.85f) * 100.0f);
    if (quality < 0) quality = 0;
    if (quality > 100) quality = 100;

    jbyteArray result = static_cast<jbyteArray>(
        env->CallStaticObjectMethod(g_heicClass, g_encodeMethod,
                                     jpixels,
                                     static_cast<jint>(img->width),
                                     static_cast<jint>(img->height),
                                     static_cast<jint>(quality)));
    env->DeleteLocalRef(jpixels);

    if (!result || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return IC_ERROR_ENCODE_FAILED;
    }

    jsize result_len = env->GetArrayLength(result);
    *out_data = static_cast<uint8_t*>(malloc(static_cast<size_t>(result_len)));
    if (!*out_data) {
        env->DeleteLocalRef(result);
        return IC_ERROR_ALLOC_FAILED;
    }

    env->GetByteArrayRegion(result, 0, result_len,
                            reinterpret_cast<jbyte*>(*out_data));
    *out_len = static_cast<size_t>(result_len);
    env->DeleteLocalRef(result);

    return IC_OK;
}

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int platform_heic_get_info(const uint8_t* data, size_t len,
                                       ICImageInfo* out) {
    JNIEnv* env = get_env();
    if (!env || !g_heicClass || !g_getInfoMethod)
        return IC_ERROR_UNSUPPORTED_FORMAT;

    jbyteArray jdata = env->NewByteArray(static_cast<jsize>(len));
    if (!jdata) return IC_ERROR_ALLOC_FAILED;
    env->SetByteArrayRegion(jdata, 0, static_cast<jsize>(len),
                            reinterpret_cast<const jbyte*>(data));

    jintArray result = static_cast<jintArray>(
        env->CallStaticObjectMethod(g_heicClass, g_getInfoMethod, jdata));
    env->DeleteLocalRef(jdata);

    if (!result || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return IC_ERROR_DECODE_FAILED;
    }

    jint* dims = env->GetIntArrayElements(result, nullptr);
    out->width = static_cast<uint32_t>(dims[0]);
    out->height = static_cast<uint32_t>(dims[1]);
    out->format = IC_FORMAT_HEIC;
    out->has_exif = 0;
    env->ReleaseIntArrayElements(result, dims, 0);
    env->DeleteLocalRef(result);

    return IC_OK;
}
