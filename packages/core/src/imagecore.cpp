#include "imagecore.h"

#include <cstdlib>
#include <cstring>

#define IMAGECORE_VERSION "0.1.0"

/* ── Forward declarations for codec functions ────────────────────────── */

extern "C" {

/* JPEG */
extern int jpeg_decode(const uint8_t* data, size_t len, ICImage* out);
extern int jpeg_encode(const ICImage* img, const ICEncodeOpts* opts,
                       uint8_t** out_data, size_t* out_len);
extern int jpeg_get_info(const uint8_t* data, size_t len, ICImageInfo* out);
extern int jpeg_lossless_rotate(const uint8_t* data, size_t len,
                                ICRotation rotation,
                                uint8_t** out_data, size_t* out_len);
extern int jpeg_lossless_crop(const uint8_t* data, size_t len,
                              const ICCropRegion* region,
                              uint8_t** out_data, size_t* out_len);
extern int jpeg_strip_exif(const uint8_t* data, size_t len,
                           uint8_t** out_data, size_t* out_len);

/* PNG */
extern int png_decode(const uint8_t* data, size_t len, ICImage* out);
extern int png_encode(const ICImage* img, const ICEncodeOpts* opts,
                      uint8_t** out_data, size_t* out_len);
extern int png_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

/* WebP */
extern int webp_decode(const uint8_t* data, size_t len, ICImage* out);
extern int webp_encode(const ICImage* img, const ICEncodeOpts* opts,
                       uint8_t** out_data, size_t* out_len);
extern int webp_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

/* TIFF */
extern int tiff_decode(const uint8_t* data, size_t len, ICImage* out);
extern int tiff_encode(const ICImage* img, const ICEncodeOpts* opts,
                       uint8_t** out_data, size_t* out_len);
extern int tiff_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

/* BMP */
extern int bmp_decode(const uint8_t* data, size_t len, ICImage* out);
extern int bmp_encode(const ICImage* img, const ICEncodeOpts* opts,
                      uint8_t** out_data, size_t* out_len);
extern int bmp_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

/* AVIF */
extern int avif_decode(const uint8_t* data, size_t len, ICImage* out);
extern int avif_encode(const ICImage* img, const ICEncodeOpts* opts,
                       uint8_t** out_data, size_t* out_len);
extern int avif_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

/* HEIC */
extern int heif_codec_decode(const uint8_t* data, size_t len, ICImage* out);
extern int heif_codec_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

} /* extern "C" */

/* ── Format detection ────────────────────────────────────────────────── */

ICFormat ic_detect_format(const uint8_t* data, size_t len) {
    if (!data || len < 4) return IC_FORMAT_UNKNOWN;

    /* JPEG: FF D8 FF */
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        return IC_FORMAT_JPEG;

    /* PNG: 89 50 4E 47 0D 0A 1A 0A */
    if (len >= 8 &&
        data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A)
        return IC_FORMAT_PNG;

    /* WebP: RIFF....WEBP */
    if (len >= 12 &&
        data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P')
        return IC_FORMAT_WEBP;

    /* TIFF: II*\0 (little-endian) or MM\0* (big-endian) */
    if (len >= 4 &&
        ((data[0] == 0x49 && data[1] == 0x49 && data[2] == 0x2A && data[3] == 0x00) ||
         (data[0] == 0x4D && data[1] == 0x4D && data[2] == 0x00 && data[3] == 0x2A)))
        return IC_FORMAT_TIFF;

    /* BMP: BM */
    if (len >= 2 && data[0] == 0x42 && data[1] == 0x4D)
        return IC_FORMAT_BMP;

    /* AVIF / HEIC: ftyp box in ISO BMFF container */
    if (len >= 12 && data[4] == 'f' && data[5] == 't' && data[6] == 'y' && data[7] == 'p') {
        if (data[8] == 'a' && data[9] == 'v' && data[10] == 'i' &&
            (data[11] == 'f' || data[11] == 's'))
            return IC_FORMAT_AVIF;
        if ((data[8] == 'h' && data[9] == 'e' && data[10] == 'i' && data[11] == 'c') ||
            (data[8] == 'm' && data[9] == 'i' && data[10] == 'f' && data[11] == '1') ||
            (data[8] == 'h' && data[9] == 'e' && data[10] == 'i' && data[11] == 'x') ||
            (data[8] == 'h' && data[9] == 'e' && data[10] == 'v' && data[11] == 'c') ||
            (data[8] == 'h' && data[9] == 'e' && data[10] == 'v' && data[11] == 'x'))
            return IC_FORMAT_HEIC;
    }

    /* GIF: GIF87a or GIF89a */
    if (len >= 6 &&
        data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8' &&
        (data[4] == '7' || data[4] == '9') && data[5] == 'a')
        return IC_FORMAT_GIF;

    return IC_FORMAT_UNKNOWN;
}

/* ── Image info ──────────────────────────────────────────────────────── */

int ic_get_image_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    if (!data || !len || !out) return IC_ERROR_INVALID_INPUT;

    memset(out, 0, sizeof(ICImageInfo));
    out->file_size = len;
    out->format = ic_detect_format(data, len);

    switch (out->format) {
        case IC_FORMAT_JPEG: return jpeg_get_info(data, len, out);
        case IC_FORMAT_PNG:  return png_get_info(data, len, out);
        case IC_FORMAT_WEBP: return webp_get_info(data, len, out);
        case IC_FORMAT_TIFF: return tiff_get_info(data, len, out);
        case IC_FORMAT_BMP:  return bmp_get_info(data, len, out);
        case IC_FORMAT_AVIF: return avif_get_info(data, len, out);
        case IC_FORMAT_HEIC: return heif_codec_get_info(data, len, out);
        default:             return IC_ERROR_UNSUPPORTED_FORMAT;
    }
}

/* ── Decode ──────────────────────────────────────────────────────────── */

int ic_decode(const uint8_t* data, size_t len, ICImage* out) {
    if (!data || !len || !out) return IC_ERROR_INVALID_INPUT;

    memset(out, 0, sizeof(ICImage));
    ICFormat fmt = ic_detect_format(data, len);

    switch (fmt) {
        case IC_FORMAT_JPEG: return jpeg_decode(data, len, out);
        case IC_FORMAT_PNG:  return png_decode(data, len, out);
        case IC_FORMAT_WEBP: return webp_decode(data, len, out);
        case IC_FORMAT_TIFF: return tiff_decode(data, len, out);
        case IC_FORMAT_BMP:  return bmp_decode(data, len, out);
        case IC_FORMAT_AVIF: return avif_decode(data, len, out);
        case IC_FORMAT_HEIC: return heif_codec_decode(data, len, out);
        default:             return IC_ERROR_UNSUPPORTED_FORMAT;
    }
}

/* ── Encode ──────────────────────────────────────────────────────────── */

int ic_encode(const ICImage* img, ICFormat fmt, const ICEncodeOpts* opts,
              uint8_t** out_data, size_t* out_len) {
    if (!img || !img->pixels || !out_data || !out_len)
        return IC_ERROR_INVALID_INPUT;

    ICEncodeOpts default_opts = { 0.85f, 0, 0 };
    if (!opts) opts = &default_opts;

    switch (fmt) {
        case IC_FORMAT_JPEG: return jpeg_encode(img, opts, out_data, out_len);
        case IC_FORMAT_PNG:  return png_encode(img, opts, out_data, out_len);
        case IC_FORMAT_WEBP: return webp_encode(img, opts, out_data, out_len);
        case IC_FORMAT_TIFF: return tiff_encode(img, opts, out_data, out_len);
        case IC_FORMAT_BMP:  return bmp_encode(img, opts, out_data, out_len);
        case IC_FORMAT_AVIF: return avif_encode(img, opts, out_data, out_len);
        default:             return IC_ERROR_UNSUPPORTED_FORMAT;
    }
}

/* ── Convert ─────────────────────────────────────────────────────────── */

int ic_convert(const uint8_t* data, size_t len, ICFormat target_fmt,
               const ICEncodeOpts* opts, uint8_t** out_data, size_t* out_len) {
    ICImage img;
    int err = ic_decode(data, len, &img);
    if (err != IC_OK) return err;

    err = ic_encode(&img, target_fmt, opts, out_data, out_len);
    ic_image_free(&img);
    return err;
}

/* ── Lossless JPEG wrappers ──────────────────────────────────────────── */

int ic_jpeg_lossless_rotate(const uint8_t* data, size_t len,
                            ICRotation rotation,
                            uint8_t** out_data, size_t* out_len) {
    if (!data || !len || !out_data || !out_len)
        return IC_ERROR_INVALID_INPUT;
    if (rotation != IC_ROTATE_90 && rotation != IC_ROTATE_180 &&
        rotation != IC_ROTATE_270)
        return IC_ERROR_INVALID_ROTATION;
    if (ic_detect_format(data, len) != IC_FORMAT_JPEG)
        return IC_ERROR_UNSUPPORTED_FORMAT;

    return jpeg_lossless_rotate(data, len, rotation, out_data, out_len);
}

int ic_jpeg_lossless_crop(const uint8_t* data, size_t len,
                          const ICCropRegion* region,
                          uint8_t** out_data, size_t* out_len) {
    if (!data || !len || !region || !out_data || !out_len)
        return IC_ERROR_INVALID_INPUT;
    if (ic_detect_format(data, len) != IC_FORMAT_JPEG)
        return IC_ERROR_UNSUPPORTED_FORMAT;

    return jpeg_lossless_crop(data, len, region, out_data, out_len);
}

int ic_jpeg_strip_exif(const uint8_t* data, size_t len,
                       uint8_t** out_data, size_t* out_len) {
    if (!data || !len || !out_data || !out_len)
        return IC_ERROR_INVALID_INPUT;
    if (ic_detect_format(data, len) != IC_FORMAT_JPEG)
        return IC_ERROR_UNSUPPORTED_FORMAT;

    return jpeg_strip_exif(data, len, out_data, out_len);
}

/* ── Generic EXIF strip ──────────────────────────────────────────────── */

int ic_strip_exif(const uint8_t* data, size_t len,
                  uint8_t** out_data, size_t* out_len) {
    if (!data || !len || !out_data || !out_len)
        return IC_ERROR_INVALID_INPUT;

    ICFormat fmt = ic_detect_format(data, len);

    if (fmt == IC_FORMAT_JPEG)
        return jpeg_strip_exif(data, len, out_data, out_len);

    /* For other formats, decode and re-encode without metadata */
    ICImage img;
    int err = ic_decode(data, len, &img);
    if (err != IC_OK) return err;

    ICEncodeOpts opts = { 0.95f, 0, 1 };
    err = ic_encode(&img, fmt, &opts, out_data, out_len);
    ic_image_free(&img);
    return err;
}

/* ── Memory management ───────────────────────────────────────────────── */

void ic_free(void* ptr) {
    free(ptr);
}

void ic_image_free(ICImage* img) {
    if (img && img->pixels) {
        free(img->pixels);
        img->pixels = nullptr;
        img->width = 0;
        img->height = 0;
        img->stride = 0;
    }
}

/* ── Version ─────────────────────────────────────────────────────────── */

const char* ic_version(void) {
    return IMAGECORE_VERSION;
}
