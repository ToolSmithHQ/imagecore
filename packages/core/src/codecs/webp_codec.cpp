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

/* ── EXIF container (extended WebP) ──────────────────────────────────── */

static inline void put_le16(uint8_t* p, uint32_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static inline void put_le24(uint8_t* p, uint32_t v) { put_le16(p, v); p[2] = (v >> 16) & 0xFF; }
static inline void put_le32(uint8_t* p, uint32_t v) { put_le24(p, v); p[3] = (v >> 24) & 0xFF; }
static inline uint32_t get_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define IC_VP8X_FLAG_ALPHA 0x10
#define IC_VP8X_FLAG_EXIF  0x08

/*
 * Rewrap a simple or extended WebP produced by libwebp's encoder as an
 * extended (VP8X) file with an EXIF chunk:
 *   RIFF <size> WEBP | VP8X (flags, canvas) | [ALPH] VP8 / VP8L | EXIF
 * Chunk order follows the container spec (EXIF after the image data).
 * Returns a malloc'd buffer, or nullptr on malformed input / OOM.
 */
static uint8_t* webp_add_exif(const uint8_t* in, size_t in_len,
                              uint32_t width, uint32_t height,
                              const uint8_t* exif, size_t exif_len,
                              size_t* out_len) {
    if (in_len < 20 || memcmp(in, "RIFF", 4) || memcmp(in + 8, "WEBP", 4))
        return nullptr;

    /* Walk the chunks once to validate and find the image payload. */
    const uint8_t* body = in + 12;           /* chunks after "WEBP" */
    size_t body_len = in_len - 12;
    bool extended = false;
    uint8_t flags = 0;
    size_t pos = 0;
    while (pos + 8 <= body_len) {
        uint32_t clen = get_le32(body + pos + 4);
        if (clen > body_len - pos - 8) return nullptr;
        const uint8_t* fourcc = body + pos;
        const uint8_t* data = body + pos + 8;
        if (pos == 0 && !memcmp(fourcc, "VP8X", 4)) {
            if (clen < 10) return nullptr;
            extended = true;
            flags = data[0];
        } else if (!memcmp(fourcc, "EXIF", 4)) {
            return nullptr; /* encoder never emits one; don't produce two */
        } else if (!memcmp(fourcc, "VP8L", 4) && !extended) {
            /* VP8L header: 0x2f, then 14b width-1, 14b height-1, 1b alpha_is_used */
            if (clen < 5 || data[0] != 0x2f) return nullptr;
            if ((data[4] >> 4) & 1) flags |= IC_VP8X_FLAG_ALPHA;
        } else if (!memcmp(fourcc, "ALPH", 4)) {
            flags |= IC_VP8X_FLAG_ALPHA;
        }
        pos += 8 + clen + (clen & 1);
    }
    if (pos != body_len) return nullptr; /* trailing bytes / missing pad */

    const size_t vp8x_len = extended ? 0 : 8 + 10;
    const size_t exif_chunk = 8 + exif_len + (exif_len & 1);
    const size_t total = 12 + vp8x_len + body_len + exif_chunk;
    if (exif_len > 0xFFFFFFF0u || total - 8 > 0xFFFFFFFEu) return nullptr;

    uint8_t* out = static_cast<uint8_t*>(malloc(total));
    if (!out) return nullptr;

    uint8_t* w = out;
    memcpy(w, "RIFF", 4);
    put_le32(w + 4, static_cast<uint32_t>(total - 8));
    memcpy(w + 8, "WEBP", 4);
    w += 12;

    if (!extended) {
        memcpy(w, "VP8X", 4);
        put_le32(w + 4, 10);
        w[8] = flags | IC_VP8X_FLAG_EXIF;
        w[9] = w[10] = w[11] = 0;
        put_le24(w + 12, width - 1);
        put_le24(w + 15, height - 1);
        w += vp8x_len;
        memcpy(w, body, body_len);
    } else {
        memcpy(w, body, body_len);
        w[8] |= IC_VP8X_FLAG_EXIF; /* existing VP8X flags byte */
    }
    w += body_len;

    memcpy(w, "EXIF", 4);
    put_le32(w + 4, static_cast<uint32_t>(exif_len));
    memcpy(w + 8, exif, exif_len);
    if (exif_len & 1) w[8 + exif_len] = 0; /* pad to even size */

    *out_len = total;
    return out;
}

/* ── Encode ──────────────────────────────────────────────────────────── */

extern "C" int webp_encode(const ICImage* img, const ICEncodeOpts* opts,
                           const uint8_t* exif, size_t exif_len,
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
        WebPFree(output);
        return IC_ERROR_ENCODE_FAILED;
    }

    if (exif && exif_len > 0) {
        size_t n = 0;
        uint8_t* wrapped = webp_add_exif(output, output_size, img->width,
                                         img->height, exif, exif_len, &n);
        WebPFree(output);
        if (!wrapped) return IC_ERROR_ENCODE_FAILED;
        *out_data = wrapped;
        *out_len = n;
        return IC_OK;
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
