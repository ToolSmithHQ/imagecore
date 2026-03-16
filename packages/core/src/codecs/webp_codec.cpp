#include "../imagecore.h"

#include <cstdlib>
#include <cstring>
#include <webp/decode.h>
#include <webp/encode.h>

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int webp_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    int width, height;
    if (!WebPGetInfo(data, len, &width, &height)) {
        return IC_ERROR_DECODE_FAILED;
    }

    out->width = static_cast<uint32_t>(width);
    out->height = static_cast<uint32_t>(height);
    out->format = IC_FORMAT_WEBP;
    out->has_exif = 0; /* TODO: parse RIFF chunks for EXIF */
    return IC_OK;
}

/* ── Decode ──────────────────────────────────────────────────────────── */

extern "C" int webp_decode(const uint8_t* data, size_t len, ICImage* out) {
    int width, height;

    uint8_t* rgba = WebPDecodeRGBA(data, len, &width, &height);
    if (!rgba) {
        return IC_ERROR_DECODE_FAILED;
    }

    out->width = static_cast<uint32_t>(width);
    out->height = static_cast<uint32_t>(height);
    out->stride = static_cast<uint32_t>(width) * 4;

    /* WebPDecodeRGBA allocates with WebPFree, we need malloc for ic_image_free */
    size_t total = (size_t)out->stride * out->height;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) {
        WebPFree(rgba);
        return IC_ERROR_ALLOC_FAILED;
    }

    memcpy(out->pixels, rgba, total);
    WebPFree(rgba);
    return IC_OK;
}

/* ── Encode ──────────────────────────────────────────────────────────── */

extern "C" int webp_encode(const ICImage* img, const ICEncodeOpts* opts,
                           uint8_t** out_data, size_t* out_len) {
    uint8_t* output = nullptr;
    size_t output_size = 0;

    if (opts && opts->lossless) {
        output_size = WebPEncodeLosslessRGBA(
            img->pixels, static_cast<int>(img->width),
            static_cast<int>(img->height),
            static_cast<int>(img->stride), &output);
    } else {
        float quality = (opts ? opts->quality : 0.85f) * 100.0f;
        if (quality < 0.0f) quality = 0.0f;
        if (quality > 100.0f) quality = 100.0f;

        output_size = WebPEncodeRGBA(
            img->pixels, static_cast<int>(img->width),
            static_cast<int>(img->height),
            static_cast<int>(img->stride), quality, &output);
    }

    if (!output || output_size == 0) {
        return IC_ERROR_ENCODE_FAILED;
    }

    /* Copy to malloc'd buffer for consistency with ic_free */
    *out_data = static_cast<uint8_t*>(malloc(output_size));
    if (!*out_data) {
        WebPFree(output);
        return IC_ERROR_ALLOC_FAILED;
    }

    memcpy(*out_data, output, output_size);
    *out_len = output_size;
    WebPFree(output);
    return IC_OK;
}
