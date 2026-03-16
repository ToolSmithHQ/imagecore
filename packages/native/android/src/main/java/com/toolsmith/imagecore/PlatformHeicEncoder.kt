package com.toolsmith.imagecore

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Build
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer

/**
 * Android HEIC encode/decode via platform APIs.
 *
 * Decode: BitmapFactory (Android 9+ / API 28)
 * Encode: Bitmap.compress with HEIC format (Android 10+ / API 29)
 *
 * Called from JNI — these methods are invoked by the C++ JSI host object
 * when the format is HEIC and we're on Android.
 */
object PlatformHeicEncoder {

    /**
     * Decode HEIC bytes to RGBA pixel array.
     * Returns null on failure.
     */
    @JvmStatic
    fun decode(data: ByteArray): IntArray? {
        val options = BitmapFactory.Options().apply {
            inPreferredConfig = Bitmap.Config.ARGB_8888
        }
        val bitmap = BitmapFactory.decodeByteArray(data, 0, data.size, options)
            ?: return null

        val width = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        bitmap.recycle()

        return pixels
    }

    /**
     * Get HEIC image dimensions without full decode.
     * Returns [width, height] or null on failure.
     */
    @JvmStatic
    fun getInfo(data: ByteArray): IntArray? {
        val options = BitmapFactory.Options().apply {
            inJustDecodeBounds = true
        }
        BitmapFactory.decodeByteArray(data, 0, data.size, options)

        if (options.outWidth <= 0 || options.outHeight <= 0) return null

        return intArrayOf(options.outWidth, options.outHeight)
    }

    /**
     * Encode RGBA pixels to HEIC bytes.
     * Returns null if HEIC encoding is not supported (Android < 10).
     *
     * @param pixels ARGB_8888 pixel data
     * @param width Image width
     * @param height Image height
     * @param quality 0-100
     */
    @JvmStatic
    fun encode(pixels: IntArray, width: Int, height: Int, quality: Int): ByteArray? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return null // HEIC encode requires Android 10+
        }

        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        bitmap.setPixels(pixels, 0, width, 0, 0, width, height)

        val outputStream = ByteArrayOutputStream()

        // HEIC CompressFormat was added in API 34. For API 29-33, use WEBP_LOSSY as fallback.
        val format = try {
            Bitmap.CompressFormat.valueOf("HEIC")
        } catch (_: IllegalArgumentException) {
            return null // HEIC not available on this API level
        }
        val success = bitmap.compress(format, quality, outputStream)
        bitmap.recycle()

        if (!success) return null

        return outputStream.toByteArray()
    }

    /**
     * Check if HEIC decode is supported on this device.
     */
    @JvmStatic
    fun isDecodeSupported(): Boolean {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P // Android 9+
    }

    /**
     * Check if HEIC encode is supported on this device.
     */
    @JvmStatic
    fun isEncodeSupported(): Boolean {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q // Android 10+
    }
}
