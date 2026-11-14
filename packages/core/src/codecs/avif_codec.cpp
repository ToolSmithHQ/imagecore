#include "../imagecore.h"

#include <cstdlib>
#include <cstring>
#include <avif/avif.h>

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int avif_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    avifDecoder* decoder = avifDecoderCreate();
    if (!decoder) return IC_ERROR_ALLOC_FAILED;

    (void)avifDecoderSetIOMemory(decoder, data, len);
    decoder->ignoreExif = AVIF_FALSE;

    avifResult result = avifDecoderParse(decoder);
    int malformed_exif = 0;
    if (result == AVIF_RESULT_INVALID_EXIF_PAYLOAD) {
        /* Some writers store a wrong Exif tiff_header_offset. libavif then fails the
         * whole parse; the image itself is fine, so re-parse ignoring Exif. */
        malformed_exif = 1;
        avifDecoderDestroy(decoder);
        decoder = avifDecoderCreate();
        if (!decoder) return IC_ERROR_ALLOC_FAILED;
        (void)avifDecoderSetIOMemory(decoder, data, len);
        decoder->ignoreExif = AVIF_TRUE;
        result = avifDecoderParse(decoder);
    }
    if (result != AVIF_RESULT_OK) {
        avifDecoderDestroy(decoder);
        return IC_ERROR_DECODE_FAILED;
    }

    out->width = decoder->image->width;
    out->height = decoder->image->height;
    out->format = IC_FORMAT_AVIF;
    out->has_exif = (malformed_exif || decoder->image->exif.size > 0) ? 1 : 0;

    avifDecoderDestroy(decoder);
    return IC_OK;
}

/* ── Decode ──────────────────────────────────────────────────────────── */

extern "C" int avif_decode(const uint8_t* data, size_t len, ICImage* out) {
    avifDecoder* decoder = avifDecoderCreate();
    if (!decoder) return IC_ERROR_ALLOC_FAILED;

    (void)avifDecoderSetIOMemory(decoder, data, len);
    /* Pixels don't need metadata, and a malformed Exif/XMP payload would otherwise
     * make libavif reject an otherwise valid image. */
    decoder->ignoreExif = AVIF_TRUE;
    decoder->ignoreXMP = AVIF_TRUE;

    avifResult result = avifDecoderParse(decoder);
    if (result != AVIF_RESULT_OK) {
        avifDecoderDestroy(decoder);
        return IC_ERROR_DECODE_FAILED;
    }

    result = avifDecoderNextImage(decoder);
    if (result != AVIF_RESULT_OK) {
        avifDecoderDestroy(decoder);
        return IC_ERROR_DECODE_FAILED;
    }

    avifImage* image = decoder->image;
    out->width = image->width;
    out->height = image->height;
    out->stride = image->width * 4;

    size_t total = (size_t)out->stride * out->height;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) {
        avifDecoderDestroy(decoder);
        return IC_ERROR_ALLOC_FAILED;
    }

    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    rgb.depth = 8;
    rgb.pixels = out->pixels;
    rgb.rowBytes = out->stride;

    result = avifImageYUVToRGB(image, &rgb);
    if (result != AVIF_RESULT_OK) {
        free(out->pixels);
        out->pixels = nullptr;
        avifDecoderDestroy(decoder);
        return IC_ERROR_DECODE_FAILED;
    }

    avifDecoderDestroy(decoder);
    return IC_OK;
}

/* ── Encode ──────────────────────────────────────────────────────────── */

extern "C" int avif_encode(const ICImage* img, const ICEncodeOpts* opts,
                           uint8_t** out_data, size_t* out_len) {
    avifImage* image = avifImageCreate(img->width, img->height, 8,
                                        AVIF_PIXEL_FORMAT_YUV444);
    if (!image) return IC_ERROR_ALLOC_FAILED;

    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    rgb.depth = 8;
    rgb.pixels = img->pixels;
    rgb.rowBytes = img->stride;

    avifResult result = avifImageRGBToYUV(image, &rgb);
    if (result != AVIF_RESULT_OK) {
        avifImageDestroy(image);
        return IC_ERROR_ENCODE_FAILED;
    }

    avifEncoder* encoder = avifEncoderCreate();
    if (!encoder) {
        avifImageDestroy(image);
        return IC_ERROR_ALLOC_FAILED;
    }

    /* Map quality 0.0-1.0 to AVIF quality 0-100 (higher = better) */
    int quality = 63; /* default */
    if (opts) {
        quality = static_cast<int>(opts->quality * 100.0f);
        if (quality < 0) quality = 0;
        if (quality > 100) quality = 100;

        if (opts->lossless) {
            quality = AVIF_QUALITY_LOSSLESS;
        }
    }

    encoder->quality = quality;
    encoder->qualityAlpha = quality;

    /* Speed: 6 is a good balance for interactive use (0=slowest, 10=fastest) */
    encoder->speed = 6;

    avifRWData output = AVIF_DATA_EMPTY;
    result = avifEncoderWrite(encoder, image, &output);

    avifEncoderDestroy(encoder);
    avifImageDestroy(image);

    if (result != AVIF_RESULT_OK) {
        avifRWDataFree(&output);
        return IC_ERROR_ENCODE_FAILED;
    }

    /* Copy to malloc'd buffer for consistency with ic_free */
    *out_data = static_cast<uint8_t*>(malloc(output.size));
    if (!*out_data) {
        avifRWDataFree(&output);
        return IC_ERROR_ALLOC_FAILED;
    }

    memcpy(*out_data, output.data, output.size);
    *out_len = output.size;
    avifRWDataFree(&output);
    return IC_OK;
}

/* ── EXIF ────────────────────────────────────────────────────────────── */

extern "C" int ic_exif_copy_payload(const uint8_t* p, size_t n,
                                    uint8_t** out_data, size_t* out_len);

extern "C" int avif_get_exif(const uint8_t* data, size_t len,
                             uint8_t** out_data, size_t* out_len) {
    avifDecoder* decoder = avifDecoderCreate();
    if (!decoder) return IC_ERROR_ALLOC_FAILED;
    (void)avifDecoderSetIOMemory(decoder, data, len);
    decoder->ignoreExif = AVIF_FALSE;

    avifResult parsed = avifDecoderParse(decoder);
    if (parsed != AVIF_RESULT_OK) {
        avifDecoderDestroy(decoder);
        /* Exif present but unreadable (bad header offset): report "no usable EXIF"
         * rather than claiming the whole file is corrupt. */
        return parsed == AVIF_RESULT_INVALID_EXIF_PAYLOAD ? IC_ERROR_EXIF_NOT_FOUND
                                                          : IC_ERROR_DECODE_FAILED;
    }

    const avifRWData* exif = &decoder->image->exif;
    int err = IC_ERROR_EXIF_NOT_FOUND;
    if (exif->size > 0) {
        size_t offset = 0;
        if (avifGetExifTiffHeaderOffset(exif->data, exif->size, &offset) == AVIF_RESULT_OK)
            err = ic_exif_copy_payload(exif->data + offset, exif->size - offset,
                                       out_data, out_len);
    }
    avifDecoderDestroy(decoder);
    return err;
}
