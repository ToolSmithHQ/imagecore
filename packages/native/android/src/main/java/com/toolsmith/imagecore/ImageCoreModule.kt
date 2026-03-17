package com.toolsmith.imagecore

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.ReactMethod
import com.facebook.react.module.annotations.ReactModule

@ReactModule(name = ImageCoreModule.NAME)
class ImageCoreModule(reactContext: ReactApplicationContext) :
    ReactContextBaseJavaModule(reactContext) {

    companion object {
        const val NAME = "ImageCore"

        init {
            try {
                System.loadLibrary("imagecore-jni")
            } catch (_: UnsatisfiedLinkError) {
                // Native library not available
            }
        }
    }

    override fun getName(): String = NAME

    @ReactMethod(isBlockingSynchronousMethod = true)
    fun install(): Boolean {
        return try {
            val jsContextPtr = reactApplicationContext.javaScriptContextHolder?.get() ?: 0L
            if (jsContextPtr == 0L) return false
            nativeInstall(jsContextPtr)
            true
        } catch (_: Exception) {
            false
        }
    }

    private external fun nativeInstall(jsiRuntimePtr: Long)
}
