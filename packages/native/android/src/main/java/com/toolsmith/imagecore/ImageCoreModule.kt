package com.toolsmith.imagecore

import android.util.Log
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.ReactMethod
import com.facebook.react.module.annotations.ReactModule

@ReactModule(name = ImageCoreModule.NAME)
class ImageCoreModule(reactContext: ReactApplicationContext) :
    ReactContextBaseJavaModule(reactContext) {

    companion object {
        const val NAME = "ImageCore"
        private const val TAG = "ImageCore"

        init {
            try {
                System.loadLibrary("imagecore-jni")
                Log.d(TAG, "Loaded libimagecore-jni.so")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "Failed to load libimagecore-jni.so", e)
            }
        }
    }

    override fun getName(): String = NAME

    @ReactMethod(isBlockingSynchronousMethod = true)
    fun install(): Boolean {
        return try {
            val context = reactApplicationContext
            val jsContextPtr = context.javaScriptContextHolder?.get() ?: 0L

            if (jsContextPtr == 0L) {
                Log.e(TAG, "JS context pointer is 0 — runtime not ready")
                return false
            }

            Log.d(TAG, "Installing JSI host object with context ptr: $jsContextPtr")
            nativeInstall(jsContextPtr)
            Log.d(TAG, "JSI host object installed successfully")
            true
        } catch (e: Exception) {
            Log.e(TAG, "install() failed", e)
            false
        }
    }

    private external fun nativeInstall(jsiRuntimePtr: Long)
}
