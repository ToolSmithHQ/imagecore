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
    IC_FORMAT_HEIC,   /* WASM: libheif (libde265 decode, kvazaar encode)  */
                      /* Mobile: platform API (core returns unsupported)  */
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

/* ── Orientation ─────────────────────────────────────────────────────────
 *
 * Photos are often stored sideways with an orientation flag saying how to
 * display them. ImageCore works in *displayed* orientation everywhere:
 *   - ic_decode returns upright pixels and ic_get_image_info reports the
 *     displayed width/height. Sources: EXIF Orientation (JPEG, PNG eXIf,
 *     WebP EXIF), the TIFF Orientation tag, AVIF irot/imir. HEIC irot/imir
 *     are applied by libheif; EXIF orientation is ignored for HEIF/AVIF, as
 *     their spec requires.
 *   - ic_rotate/ic_crop/ic_resize/ic_flip_* operate on those upright pixels.
 *   - Lossless JPEG rotate/crop take rotations/regions relative to the image
 *     as displayed.
 *   - ic_get_exif returns the source's EXIF unchanged (Orientation included).
 *     Since decoded pixels are already upright, EXIF embedded into a
 *     re-encoded output must say Orientation=1: pass it through
 *     ic_exif_for_output() before ic_encode_with_exif().
 * ─────────────────────────────────────────────────────────────────────── */

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
 * width/height are as displayed: swapped when the orientation (EXIF 5–8,
 * TIFF Orientation 5–8, AVIF/HEIC irot 90/270) turns the image sideways.
 */
int ic_get_image_info(const uint8_t* data, size_t len, ICImageInfo* out);

/* ── Decode / Encode ─────────────────────────────────────────────────── */

/**
 * Decode any supported format to RGBA pixels, as displayed: the image's
 * orientation (see "Orientation" above) is applied, so a photo stored
 * sideways comes out upright with width/height swapped accordingly.
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
 * Like ic_encode, but embeds `exif` (raw TIFF-structure EXIF bytes starting
 * with "II*\0" or "MM\0*", exactly as ic_get_exif returns them; NULL/0 = none)
 * into the output. Except for TIFF, the bytes are written verbatim (not
 * parsed or patched), so ic_get_exif on the output returns them unchanged.
 *   JPEG: APP1 "Exif\0\0" segment. Skipped (still IC_OK) when the block is
 *         larger than 65527 bytes (APP1 payload limit 65533 minus the
 *         6-byte "Exif\0\0" header).
 *   PNG:  eXIf chunk before IDAT.
 *   WebP: EXIF chunk in an extended (VP8X) container.
 *   AVIF: Exif item (where AVIF is supported).
 *   HEIC: Exif item on the primary image (where HEIC encode is supported).
 *   TIFF: EXIF is TIFF IFDs, so the block is merged: descriptive IFD0 tags
 *         (Make, Artist, ...) join the image IFD, Exif/GPS/Interop IFDs
 *         become sub-IFDs. Values are kept exactly; MakerNote and IFD1
 *         (thumbnail) are dropped. If the block can't be parsed the file
 *         is written without EXIF (still IC_OK).
 *   BMP:  no metadata support; EXIF ignored.
 * Pixels are taken as upright, so `exif` should carry Orientation=1 (use
 * ic_exif_for_output); for AVIF any Exif orientation is never turned into
 * irot/imir, and libheif itself resets it to 1 for HEIC.
 * opts->strip_exif is ignored. Returns IC_ERROR_INVALID_INPUT if exif is
 * non-NULL but does not start with a TIFF header.
 * Caller must free out_data with ic_free().
 */
int ic_encode_with_exif(const ICImage* img, ICFormat fmt, const ICEncodeOpts* opts,
                        const uint8_t* exif, size_t exif_len,
                        uint8_t** out_data, size_t* out_len);

/**
 * Convenience: decode + encode in one call (format conversion).
 * The output is upright (orientation applied) and carries no metadata.
 * Caller must free out_data with ic_free().
 */
int ic_convert(const uint8_t* data, size_t len, ICFormat target_fmt,
               const ICEncodeOpts* opts, uint8_t** out_data, size_t* out_len);

/* ── Lossless JPEG operations ────────────────────────────────────────── */

/**
 * Lossless JPEG rotation (90/180/270 clockwise, relative to the image as
 * displayed). Rearranges DCT blocks without decoding pixels. Zero quality
 * loss. Partial MCU blocks at edges that would move to the top/left are
 * trimmed, so the output may be a few pixels smaller.
 * With EXIF Orientation != 1 the orientation and the rotation are composed
 * into a single lossless transform (rotate/flip/transpose/transverse) and
 * the copied EXIF gets Orientation=1 and updated PixelX/YDimension.
 * Caller must free out_data with ic_free().
 */
int ic_jpeg_lossless_rotate(const uint8_t* data, size_t len,
                            ICRotation rotation,
                            uint8_t** out_data, size_t* out_len);

/**
 * Lossless JPEG crop at MCU boundaries.
 * Adjusts region to nearest MCU boundary. Zero quality loss for aligned crops.
 * `region` is in displayed coordinates. With EXIF Orientation != 1 it is
 * mapped onto the stored pixels and the orientation tag is kept (the region
 * must then lie within the image, else IC_ERROR_INVALID_CROP); the MCU
 * alignment can grow the region on any side as displayed.
 * Caller must free out_data with ic_free().
 */
int ic_jpeg_lossless_crop(const uint8_t* data, size_t len,
                          const ICCropRegion* region,
                          uint8_t** out_data, size_t* out_len);

/**
 * Strip EXIF from JPEG by binary removal of APP1 marker segment.
 * No pixel re-encoding. Output is never larger than input.
 * If the EXIF Orientation is not 1, a minimal EXIF APP1 holding only the
 * Orientation tag is kept in its place, so the photo still displays upright.
 * Caller must free out_data with ic_free().
 */
int ic_jpeg_strip_exif(const uint8_t* data, size_t len,
                       uint8_t** out_data, size_t* out_len);

/* ── Pixel operations ──────────────────────────────────────── */

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
 * JPEG: as ic_jpeg_strip_exif (orientation-only EXIF kept when needed).
 * Other formats: decode + encode without metadata. Decoding applies the
 * orientation, so the output is upright with no orientation tag. PNG stays
 * lossless, a lossless (VP8L) WebP stays lossless, lossy formats re-encode
 * at quality 0.95.
 * Caller must free out_data with ic_free().
 */
int ic_strip_exif(const uint8_t* data, size_t len,
                  uint8_t** out_data, size_t* out_len);

/**
 * Extract the raw EXIF block as a TIFF structure (starts with "II*\0" or
 * "MM\0*"; any "Exif\0\0" prefix is stripped). Supports JPEG (APP1),
 * PNG (eXIf), WebP (EXIF chunk), TIFF (IFD0 descriptive tags + Exif/GPS
 * sub-IFDs rebuilt as a compact block, no pixel data), AVIF (when built with
 * libavif) and HEIC (when built with libheif).
 * Returns IC_ERROR_EXIF_NOT_FOUND when the image carries no EXIF.
 * Caller must free out_data with ic_free().
 */
int ic_get_exif(const uint8_t* data, size_t len,
                uint8_t** out_data, size_t* out_len);

/**
 * Prepare a source image's EXIF (as returned by ic_get_exif; an "Exif\0\0"
 * prefix is accepted and dropped) for embedding in a re-encoded output via
 * ic_encode_with_exif: returns a copy with Orientation set to 1 (decoded
 * pixels are already upright) and PixelXDimension/PixelYDimension set to
 * width/height (the output size after resize/crop/rotate). Values are
 * patched in place (inline SHORT/LONG entries in IFD0 and the Exif IFD), so
 * the structure and every other tag are unchanged; absent tags are not added.
 * Returns IC_ERROR_INVALID_INPUT if `exif` isn't a readable TIFF/EXIF block.
 * Caller must free *out with ic_free().
 */
int ic_exif_for_output(const uint8_t* exif, size_t len,
                       uint32_t width, uint32_t height,
                       uint8_t** out, size_t* out_len);

/* ── Memory management ───────────────────────────────────────────────── */

void ic_free(void* ptr);
void ic_image_free(ICImage* img);

/* ── Version ─────────────────────────────────────────────────────────── */

const char* ic_version(void);

#ifdef __cplusplus
}
#endif

#endif /* IMAGECORE_H */
