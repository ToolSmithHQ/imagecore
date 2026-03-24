package com.toolsmith.imagecore

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.heifwriter.HeifWriter
import java.io.File

/**
 * Android HEIC encode/decode via platform APIs.
 *
 * Decode: BitmapFactory (Android 9+)
 * Encode: HeifWriter from androidx.heifwriter (Android 9+)
 *         Handles RGB→YUV conversion and HEIF container internally.
 */
object PlatformHeicEncoder {

    private const val TAG = "PlatformHeicEncoder"

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

    @JvmStatic
    fun getInfo(data: ByteArray): IntArray? {
        val options = BitmapFactory.Options().apply {
            inJustDecodeBounds = true
        }
        BitmapFactory.decodeByteArray(data, 0, data.size, options)

        if (options.outWidth <= 0 || options.outHeight <= 0) return null

        return intArrayOf(options.outWidth, options.outHeight)
    }

    @JvmStatic
    fun encode(pixels: IntArray, width: Int, height: Int, quality: Int): ByteArray? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return null // HeifWriter requires API 28+
        }

        return try {
            encodeViaHeifWriter(pixels, width, height, quality)
        } catch (e: Exception) {
            Log.e(TAG, "HEIC encode failed", e)
            null
        }
    }

    private fun encodeViaHeifWriter(pixels: IntArray, width: Int, height: Int, quality: Int): ByteArray? {
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        bitmap.setPixels(pixels, 0, width, 0, 0, width, height)

        val tempFile = File.createTempFile("heic_encode_", ".heic")

        try {
            val writer = HeifWriter.Builder(
                tempFile.absolutePath,
                width,
                height,
                HeifWriter.INPUT_MODE_BITMAP
            )
                .setQuality(quality.coerceIn(0, 100))
                .setMaxImages(1)
                .setHandler(Handler(Looper.getMainLooper()))
                .build()

            writer.start()
            writer.addBitmap(bitmap)
            writer.stop(5000) // 5 second timeout
            writer.close()

            bitmap.recycle()

            val result = tempFile.readBytes()
            tempFile.delete()

            return if (result.isNotEmpty()) result else null
        } catch (e: Exception) {
            Log.e(TAG, "HeifWriter encode error", e)
            bitmap.recycle()
            tempFile.delete()
            return null
        }
    }

    @JvmStatic
    fun isDecodeSupported(): Boolean {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
    }

    @JvmStatic
    fun isEncodeSupported(): Boolean {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
    }
}
