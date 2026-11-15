/*
 * HEIF/HEIC codec — WASM only (LGPL libheif + libde265 decode,
 * BSD-3 kvazaar encode; all statically linked, single-threaded).
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

/* ── Encode (HEVC via kvazaar) ───────────────────────────────────────── */

struct ICHeifMemWriter {
    uint8_t* data = nullptr;
    size_t len = 0;
};

static heif_error heif_mem_write(heif_context*, const void* data, size_t size, void* userdata) {
    auto* w = static_cast<ICHeifMemWriter*>(userdata);
    w->data = static_cast<uint8_t*>(malloc(size ? size : 1));
    if (!w->data)
        return { heif_error_Memory_allocation_error, heif_suberror_Unspecified, "out of memory" };
    memcpy(w->data, data, size);
    w->len = size;
    return { heif_error_Ok, heif_suberror_Unspecified, "" };
}

extern "C" int heif_codec_encode(const ICImage* img, const ICEncodeOpts* opts,
                                 const uint8_t* exif, size_t exif_len,
                                 uint8_t** out_data, size_t* out_len) {
    if (!img->width || !img->height || img->stride < img->width * 4)
        return IC_ERROR_INVALID_INPUT;

    /* Only carry an alpha plane when the image actually has transparency. */
    bool has_alpha = false;
    for (uint32_t y = 0; y < img->height && !has_alpha; y++) {
        const uint8_t* row = img->pixels + (size_t)y * img->stride;
        for (uint32_t x = 0; x < img->width; x++) {
            if (row[x * 4 + 3] != 0xFF) { has_alpha = true; break; }
        }
    }

    heif_image* image = nullptr;
    heif_error err = heif_image_create((int)img->width, (int)img->height,
                                       heif_colorspace_RGB,
                                       has_alpha ? heif_chroma_interleaved_RGBA
                                                 : heif_chroma_interleaved_RGB,
                                       &image);
    if (err.code != heif_error_Ok) return IC_ERROR_ALLOC_FAILED;

    err = heif_image_add_plane(image, heif_channel_interleaved,
                               (int)img->width, (int)img->height, 8);
    if (err.code != heif_error_Ok) {
        heif_image_release(image);
        return IC_ERROR_ALLOC_FAILED;
    }

    int dst_stride = 0;
    uint8_t* dst = heif_image_get_plane(image, heif_channel_interleaved, &dst_stride);
    for (uint32_t y = 0; y < img->height; y++) {
        const uint8_t* s = img->pixels + (size_t)y * img->stride;
        uint8_t* d = dst + (size_t)y * dst_stride;
        if (has_alpha) {
            memcpy(d, s, (size_t)img->width * 4);
        } else {
            for (uint32_t x = 0; x < img->width; x++) {
                d[x * 3 + 0] = s[x * 4 + 0];
                d[x * 3 + 1] = s[x * 4 + 1];
                d[x * 3 + 2] = s[x * 4 + 2];
            }
        }
    }

    heif_context* ctx = heif_context_alloc();
    heif_encoder* encoder = nullptr;
    int result = IC_ERROR_ENCODE_FAILED;
    if (!ctx) {
        heif_image_release(image);
        return IC_ERROR_ALLOC_FAILED;
    }

    err = heif_context_get_encoder_for_format(ctx, heif_compression_HEVC, &encoder);
    if (err.code != heif_error_Ok || !encoder) {
        result = IC_ERROR_UNSUPPORTED_FORMAT; /* no HEVC encoder compiled in */
        goto done;
    }

    {
        /* libheif's kvazaar plugin maps quality Q to QP = (100-Q)*51/100,
         * so a straight q*100 gives QP 8 at q=0.85: ~2x the bytes of AVIF
         * or JPEG for no visible gain (4:2:0 output plateaus ~42 dB PSNR).
         * Scale to Q = q*65 so q=0.85 -> QP ~23 and q=1.0 -> QP ~18, which
         * is typical camera-HEIC quality and matches AVIF/JPEG sizes. */
        float q = opts ? opts->quality : 0.85f;
        if (q < 0.0f) q = 0.0f;
        if (q > 1.0f) q = 1.0f;
        int quality = (int)(q * 65.0f + 0.5f);
        /* libheif's kvazaar plugin always converts to YCbCr 4:2:0, so "lossless"
         * HEVC would still lose chroma. Refuse instead of silently degrading. */
        if (opts && opts->lossless) { result = IC_ERROR_UNSUPPORTED_FORMAT; goto done; }
        heif_encoder_set_lossy_quality(encoder, quality);

        heif_encoding_options* eo = heif_encoding_options_alloc();
        eo->save_alpha_channel = has_alpha ? 1 : 0;
        heif_image_handle* handle = nullptr;
        err = heif_context_encode_image(ctx, image, encoder, eo, &handle);
        heif_encoding_options_free(eo);
        if (err.code != heif_error_Ok) {
            if (handle) heif_image_handle_release(handle);
            goto done;
        }
        if (exif && exif_len > 0) {
            /* Adds an 'Exif' item (4-byte tiff_header_offset + TIFF) with a
             * cdsc reference to the primary image. */
            err = heif_context_add_exif_metadata(ctx, handle, exif, (int)exif_len);
        }
        heif_image_handle_release(handle);
        if (err.code != heif_error_Ok) goto done;
    }

    {
        ICHeifMemWriter mem;
        heif_writer writer = {};
        writer.writer_api_version = 1;
        writer.write = heif_mem_write;
        err = heif_context_write(ctx, &writer, &mem);
        if (err.code != heif_error_Ok || !mem.data) {
            free(mem.data);
            goto done;
        }
        *out_data = mem.data;
        *out_len = mem.len;
        result = IC_OK;
    }

done:
    if (encoder) heif_encoder_release(encoder);
    heif_context_free(ctx);
    heif_image_release(image);
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

extern "C" int heif_codec_encode(const ICImage*, const ICEncodeOpts*, const uint8_t*, size_t,
                                 uint8_t**, size_t*) {
    return IC_ERROR_UNSUPPORTED_FORMAT;
}

#endif /* IC_HAS_LIBHEIF */
