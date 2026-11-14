/*
 * HEIF/HEIC codec — WASM only (LGPL libheif + libde265).
 * On mobile, HEIC is handled by platform APIs (ImageIO on iOS, BitmapFactory on Android).
 *
 * Only compiled when IC_HAS_LIBHEIF is defined (set by CMake when IC_USE_LIBHEIF=ON).
 */

#include "../imagecore.h"

#ifdef IC_HAS_LIBHEIF

#include <cstdlib>
#include <cstring>
#include <libheif/heif.h>

/* ── Decode ──────────────────────────────────────────────────────────── */

extern "C" int heif_codec_decode(const uint8_t* data, size_t len, ICImage* out) {
    heif_context* ctx = heif_context_alloc();
    if (!ctx) return IC_ERROR_ALLOC_FAILED;

    heif_error err = heif_context_read_from_memory_without_copy(ctx, data, len, nullptr);
    if (err.code != heif_error_Ok) {
        heif_context_free(ctx);
        return IC_ERROR_DECODE_FAILED;
    }

    heif_image_handle* handle = nullptr;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code != heif_error_Ok) {
        heif_context_free(ctx);
        return IC_ERROR_DECODE_FAILED;
    }

    heif_image* image = nullptr;
    err = heif_decode_image(handle, &image, heif_colorspace_RGB,
                            heif_chroma_interleaved_RGBA, nullptr);
    if (err.code != heif_error_Ok) {
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return IC_ERROR_DECODE_FAILED;
    }

    int stride;
    const uint8_t* pixels = heif_image_get_plane_readonly(image,
                                                           heif_channel_interleaved,
                                                           &stride);
    if (!pixels) {
        heif_image_release(image);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return IC_ERROR_DECODE_FAILED;
    }

    out->width = static_cast<uint32_t>(heif_image_get_width(image, heif_channel_interleaved));
    out->height = static_cast<uint32_t>(heif_image_get_height(image, heif_channel_interleaved));
    out->stride = out->width * 4;

    size_t total = (size_t)out->stride * out->height;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) {
        heif_image_release(image);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return IC_ERROR_ALLOC_FAILED;
    }

    /* Copy row by row in case stride differs */
    for (uint32_t y = 0; y < out->height; y++) {
        memcpy(out->pixels + y * out->stride,
               pixels + y * stride,
               out->stride);
    }

    heif_image_release(image);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return IC_OK;
}

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int heif_codec_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    heif_context* ctx = heif_context_alloc();
    if (!ctx) return IC_ERROR_ALLOC_FAILED;

    heif_error err = heif_context_read_from_memory_without_copy(ctx, data, len, nullptr);
    if (err.code != heif_error_Ok) {
        heif_context_free(ctx);
        return IC_ERROR_DECODE_FAILED;
    }

    heif_image_handle* handle = nullptr;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code != heif_error_Ok) {
        heif_context_free(ctx);
        return IC_ERROR_DECODE_FAILED;
    }

    out->width = static_cast<uint32_t>(heif_image_handle_get_width(handle));
    out->height = static_cast<uint32_t>(heif_image_handle_get_height(handle));
    out->format = IC_FORMAT_HEIC;

    /* Check for EXIF */
    int num_metadata = heif_image_handle_get_number_of_metadata_blocks(handle, "Exif");
    out->has_exif = (num_metadata > 0) ? 1 : 0;

    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return IC_OK;
}

/* ── EXIF ────────────────────────────────────────────────────────────── */

extern "C" int ic_exif_copy_payload(const uint8_t* p, size_t n,
                                    uint8_t** out_data, size_t* out_len);

extern "C" int heif_codec_get_exif(const uint8_t* data, size_t len,
                                   uint8_t** out_data, size_t* out_len) {
    heif_context* ctx = heif_context_alloc();
    if (!ctx) return IC_ERROR_ALLOC_FAILED;

    heif_error err = heif_context_read_from_memory_without_copy(ctx, data, len, nullptr);
    heif_image_handle* handle = nullptr;
    if (err.code == heif_error_Ok)
        err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code != heif_error_Ok) {
        heif_context_free(ctx);
        return IC_ERROR_DECODE_FAILED;
    }

    int result = IC_ERROR_EXIF_NOT_FOUND;
    heif_item_id id;
    if (heif_image_handle_get_list_of_metadata_block_IDs(handle, "Exif", &id, 1) == 1) {
        size_t size = heif_image_handle_get_metadata_size(handle, id);
        uint8_t* buf = static_cast<uint8_t*>(malloc(size ? size : 1));
        if (!buf) {
            result = IC_ERROR_ALLOC_FAILED;
        } else if (heif_image_handle_get_metadata(handle, id, buf).code == heif_error_Ok &&
                   size > 4) {
            /* HEIF Exif item: 4-byte big-endian offset to the TIFF header,
               measured from the end of the offset field. */
            uint32_t off = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                           ((uint32_t)buf[2] << 8) | buf[3];
            if (off <= size - 4)
                result = ic_exif_copy_payload(buf + 4 + off, size - 4 - off,
                                              out_data, out_len);
            if (result == IC_ERROR_EXIF_NOT_FOUND) {
                /* Some writers get the offset wrong; scan for the header. */
                for (size_t i = 4; i + 8 <= size && i < 64; i++) {
                    if (ic_exif_copy_payload(buf + i, size - i, out_data, out_len) == IC_OK) {
                        result = IC_OK;
                        break;
                    }
                }
            }
        }
        free(buf);
    }

    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return result;
}

#else

/* Stubs when libheif is not available (native builds) */
extern "C" int heif_codec_decode(const uint8_t*, size_t, ICImage*) {
    return IC_ERROR_UNSUPPORTED_FORMAT;
}

extern "C" int heif_codec_get_info(const uint8_t*, size_t, ICImageInfo*) {
    return IC_ERROR_UNSUPPORTED_FORMAT;
}

extern "C" int heif_codec_get_exif(const uint8_t*, size_t, uint8_t**, size_t*) {
    return IC_ERROR_UNSUPPORTED_FORMAT;
}

#endif /* IC_HAS_LIBHEIF */
