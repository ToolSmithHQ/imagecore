/*
 * AVIF stub for platforms where libavif/libaom are not available (iOS native).
 * Returns IC_ERROR_UNSUPPORTED_FORMAT for all operations.
 * The real implementation is in avif_codec.cpp (used on macOS host + WASM).
 */

#include "../imagecore.h"

int avif_decode(const uint8_t* data, size_t len, ICImage* out) {
    (void)data; (void)len; (void)out;
    return IC_ERROR_UNSUPPORTED_FORMAT;
}

int avif_encode(const ICImage* img, const ICEncodeOpts* opts,
                uint8_t** out_data, size_t* out_len) {
    (void)img; (void)opts; (void)out_data; (void)out_len;
    return IC_ERROR_UNSUPPORTED_FORMAT;
}

int avif_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    (void)data; (void)len; (void)out;
    return IC_ERROR_UNSUPPORTED_FORMAT;
}
