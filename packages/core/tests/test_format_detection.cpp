#include "test_harness.h"

TEST(version) {
    const char* ver = ic_version();
    ASSERT(ver != nullptr);
    ASSERT(strlen(ver) > 0);
    printf("[%s] ", ver);
}

TEST(format_detection_jpeg) {
    uint8_t magic[] = { 0xFF, 0xD8, 0xFF, 0xE0 };
    ASSERT(ic_detect_format(magic, 4) == IC_FORMAT_JPEG);
}

TEST(format_detection_png) {
    uint8_t magic[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    ASSERT(ic_detect_format(magic, 8) == IC_FORMAT_PNG);
}

TEST(format_detection_webp) {
    uint8_t magic[] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P' };
    ASSERT(ic_detect_format(magic, 12) == IC_FORMAT_WEBP);
}

TEST(format_detection_bmp) {
    uint8_t magic[] = { 0x42, 0x4D, 0, 0 };
    ASSERT(ic_detect_format(magic, 4) == IC_FORMAT_BMP);
}

TEST(format_detection_tiff_le) {
    uint8_t magic[] = { 0x49, 0x49, 0x2A, 0x00 };
    ASSERT(ic_detect_format(magic, 4) == IC_FORMAT_TIFF);
}

TEST(format_detection_tiff_be) {
    uint8_t magic[] = { 0x4D, 0x4D, 0x00, 0x2A };
    ASSERT(ic_detect_format(magic, 4) == IC_FORMAT_TIFF);
}

TEST(format_detection_unknown) {
    uint8_t garbage[] = { 0x00, 0x01, 0x02, 0x03 };
    ASSERT(ic_detect_format(garbage, 4) == IC_FORMAT_UNKNOWN);
}

TEST(null_input_handling) {
    ASSERT(ic_detect_format(nullptr, 0) == IC_FORMAT_UNKNOWN);

    ICImageInfo info;
    ASSERT(ic_get_image_info(nullptr, 0, &info) == IC_ERROR_INVALID_INPUT);

    ICImage img;
    ASSERT(ic_decode(nullptr, 0, &img) == IC_ERROR_INVALID_INPUT);

    ASSERT(ic_encode(nullptr, IC_FORMAT_JPEG, nullptr, nullptr, nullptr)
           == IC_ERROR_INVALID_INPUT);
}

TEST(get_image_info_jpeg) {
    ICImage src = make_test_image(80, 60);
    uint8_t* data = nullptr; size_t len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    ASSERT(ic_encode(&src, IC_FORMAT_JPEG, &opts, &data, &len) == IC_OK);

    ICImageInfo info;
    ASSERT(ic_get_image_info(data, len, &info) == IC_OK);
    ASSERT(info.width == 80);
    ASSERT(info.height == 60);
    ASSERT(info.format == IC_FORMAT_JPEG);
    ASSERT(info.file_size == len);

    ic_free(data);
    ic_image_free(&src);
}
