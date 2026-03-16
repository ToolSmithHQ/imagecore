#ifndef IMAGECORE_H
#define IMAGECORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Error codes ─────────────────────────────────────────────────────── */

typedef enum {
    IC_OK = 0,
    IC_ERROR_INVALID_INPUT = -1,
    IC_ERROR_UNSUPPORTED_FORMAT = -2,
    IC_ERROR_DECODE_FAILED = -3,
    IC_ERROR_ENCODE_FAILED = -4,
    IC_ERROR_ALLOC_FAILED = -5,
    IC_ERROR_INVALID_ROTATION = -6,
    IC_ERROR_INVALID_CROP = -7,
    IC_ERROR_EXIF_NOT_FOUND = -8,
} ICError;

/* ── Image format enum ───────────────────────────────────────────────── */

typedef enum {
    IC_FORMAT_UNKNOWN = 0,
    IC_FORMAT_JPEG,
    IC_FORMAT_PNG,
    IC_FORMAT_WEBP,
    IC_FORMAT_TIFF,
    IC_FORMAT_BMP,
    IC_FORMAT_AVIF,
    IC_FORMAT_HEIC,   /* Decode: libheif (WASM) or platform API (mobile)  */
                      /* Encode: platform API only (mobile), not on web   */
    IC_FORMAT_GIF,
} ICFormat;

/* ── Rotation enum ───────────────────────────────────────────────────── */

typedef enum {
    IC_ROTATE_90  = 90,
    IC_ROTATE_180 = 180,
    IC_ROTATE_270 = 270,
} ICRotation;

/* ── Resize filter enum ──────────────────────────────────────────────── */

typedef enum {
    IC_FILTER_LANCZOS = 0,
    IC_FILTER_BILINEAR,
    IC_FILTER_NEAREST,
} ICResizeFilter;

/* ── Structs ─────────────────────────────────────────────────────────── */

typedef struct {
    uint32_t width;
    uint32_t height;
    ICFormat format;
    int      has_exif;
    size_t   file_size;
} ICImageInfo;

typedef struct {
    uint8_t* pixels;     /* RGBA, 4 bytes per pixel */
    uint32_t width;
    uint32_t height;
    uint32_t stride;     /* bytes per row (width * 4, may include padding) */
} ICImage;

typedef struct {
    float quality;       /* 0.0 – 1.0, for lossy formats */
    int   lossless;      /* WebP lossless mode (0 or 1) */
    int   strip_exif;    /* strip metadata on encode (0 or 1) */
} ICEncodeOpts;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} ICCropRegion;

/* ── Format detection ────────────────────────────────────────────────── */

/**
 * Detect image format from magic bytes.
 * Returns IC_FORMAT_UNKNOWN if unrecognized.
 */
ICFormat ic_detect_format(const uint8_t* data, size_t len);

/**
 * Get image info (dimensions, format, EXIF presence) without full decode.
 */
int ic_get_image_info(const uint8_t* data, size_t len, ICImageInfo* out);

/* ── Decode / Encode ─────────────────────────────────────────────────── */

/**
 * Decode any supported format to RGBA pixels.
 * Caller must free with ic_image_free().
 */
int ic_decode(const uint8_t* data, size_t len, ICImage* out);

/**
 * Encode RGBA pixels to the specified format.
 * Caller must free out_data with ic_free().
 */
int ic_encode(const ICImage* img, ICFormat fmt, const ICEncodeOpts* opts,
              uint8_t** out_data, size_t* out_len);

/**
 * Convenience: decode + encode in one call (format conversion).
 * Caller must free out_data with ic_free().
 */
int ic_convert(const uint8_t* data, size_t len, ICFormat target_fmt,
               const ICEncodeOpts* opts, uint8_t** out_data, size_t* out_len);

/* ── Lossless JPEG operations ────────────────────────────────────────── */

/**
 * Lossless JPEG rotation (90/180/270).
 * Rearranges DCT blocks without decoding pixels. Zero quality loss.
 * Caller must free out_data with ic_free().
 */
int ic_jpeg_lossless_rotate(const uint8_t* data, size_t len,
                            ICRotation rotation,
                            uint8_t** out_data, size_t* out_len);

/**
 * Lossless JPEG crop at MCU boundaries.
 * Adjusts region to nearest MCU boundary. Zero quality loss for aligned crops.
 * Caller must free out_data with ic_free().
 */
int ic_jpeg_lossless_crop(const uint8_t* data, size_t len,
                          const ICCropRegion* region,
                          uint8_t** out_data, size_t* out_len);

/**
 * Strip EXIF from JPEG by binary removal of APP1 marker segment.
 * No pixel re-encoding. Output is always smaller than input.
 * Caller must free out_data with ic_free().
 */
int ic_jpeg_strip_exif(const uint8_t* data, size_t len,
                       uint8_t** out_data, size_t* out_len);

/* ── Pixel operations (Phase 2) ──────────────────────────────────────── */

/**
 * Resize decoded image. Allocates new pixel buffer.
 * Caller must free result with ic_image_free().
 */
int ic_resize(const ICImage* img, uint32_t width, uint32_t height,
              ICResizeFilter filter, ICImage* out);

/**
 * Crop decoded image. Allocates new pixel buffer.
 * Caller must free result with ic_image_free().
 */
int ic_crop(const ICImage* img, const ICCropRegion* region, ICImage* out);

/**
 * Rotate decoded image by 90/180/270 degrees.
 * Caller must free result with ic_image_free().
 */
int ic_rotate(const ICImage* img, ICRotation rotation, ICImage* out);

/**
 * Flip decoded image horizontally or vertically.
 * Modifies pixels in place.
 */
int ic_flip_horizontal(ICImage* img);
int ic_flip_vertical(ICImage* img);

/* ── Metadata ────────────────────────────────────────────────────────── */

/**
 * Strip EXIF/metadata from any supported format (binary removal where possible).
 * Falls back to decode+encode without metadata for formats that don't support
 * binary stripping.
 * Caller must free out_data with ic_free().
 */
int ic_strip_exif(const uint8_t* data, size_t len,
                  uint8_t** out_data, size_t* out_len);

/* ── Memory management ───────────────────────────────────────────────── */

void ic_free(void* ptr);
void ic_image_free(ICImage* img);

/* ── Version ─────────────────────────────────────────────────────────── */

const char* ic_version(void);

#ifdef __cplusplus
}
#endif

#endif /* IMAGECORE_H */
