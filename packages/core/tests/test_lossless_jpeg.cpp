#include "test_harness.h"

TEST(jpeg_lossless_rotate_90) {
    ICImage src = make_test_image(64, 48);
    uint8_t* jpeg = nullptr; size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.95f, 0, 0 };
    ASSERT(ic_encode(&src, IC_FORMAT_JPEG, &opts, &jpeg, &jpeg_len) == IC_OK);

    uint8_t* rotated = nullptr; size_t rotated_len = 0;
    ASSERT(ic_jpeg_lossless_rotate(jpeg, jpeg_len, IC_ROTATE_90, &rotated, &rotated_len) == IC_OK);

    ICImageInfo info;
    ASSERT(ic_get_image_info(rotated, rotated_len, &info) == IC_OK);
    ASSERT(info.width == 48 && info.height == 64);

    double ratio = (double)rotated_len / jpeg_len;
    printf("[ratio: %.3f] ", ratio);
    ASSERT(ratio > 0.9 && ratio < 1.1);

    ic_free(jpeg); ic_free(rotated); ic_image_free(&src);
}

TEST(jpeg_binary_exif_strip) {
    ICImage src = make_test_image(32, 32);
    uint8_t* jpeg = nullptr; size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.95f, 0, 0 };
    ASSERT(ic_encode(&src, IC_FORMAT_JPEG, &opts, &jpeg, &jpeg_len) == IC_OK);

    uint8_t* stripped = nullptr; size_t stripped_len = 0;
    ASSERT(ic_jpeg_strip_exif(jpeg, jpeg_len, &stripped, &stripped_len) == IC_OK);
    ASSERT(stripped_len <= jpeg_len);
    ASSERT(ic_detect_format(stripped, stripped_len) == IC_FORMAT_JPEG);

    ic_free(jpeg); ic_free(stripped); ic_image_free(&src);
}
