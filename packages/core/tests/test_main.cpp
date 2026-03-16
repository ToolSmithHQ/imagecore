/*
 * Basic test suite for imagecore C API.
 * Tests format detection, encode/decode round-trips, and lossless JPEG ops.
 *
 * Build with: cmake -S . -B build -DIC_BUILD_TESTS=ON && cmake --build build
 * Run with:   ./build/test_imagecore
 */

#include "imagecore.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    static struct Register_##name { \
        Register_##name() { \
            printf("  TEST %-40s ", #name); \
            test_##name(); \
            printf("PASS\n"); \
            tests_passed++; \
        } \
    } register_##name; \
    static void test_##name()

#define ASSERT(cond) \
    do { if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } } while(0)

/* ── Helper: create a simple test image ──────────────────────────────── */

static ICImage make_test_image(uint32_t w, uint32_t h) {
    ICImage img;
    img.width = w;
    img.height = h;
    img.stride = w * 4;
    img.pixels = static_cast<uint8_t*>(malloc(img.stride * h));

    /* Red/green gradient */
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t* p = img.pixels + y * img.stride + x * 4;
            p[0] = static_cast<uint8_t>((x * 255) / w); /* R */
            p[1] = static_cast<uint8_t>((y * 255) / h); /* G */
            p[2] = 128;                                   /* B */
            p[3] = 255;                                   /* A */
        }
    }
    return img;
}

/* ── Tests ───────────────────────────────────────────────────────────── */

TEST(version) {
    const char* ver = ic_version();
    ASSERT(ver != nullptr);
    ASSERT(strlen(ver) > 0);
    printf("[%s] ", ver);
}

TEST(format_detection_jpeg) {
    uint8_t jpeg_magic[] = { 0xFF, 0xD8, 0xFF, 0xE0 };
    ASSERT(ic_detect_format(jpeg_magic, 4) == IC_FORMAT_JPEG);
}

TEST(format_detection_png) {
    uint8_t png_magic[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    ASSERT(ic_detect_format(png_magic, 8) == IC_FORMAT_PNG);
}

TEST(format_detection_webp) {
    uint8_t webp_magic[] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P' };
    ASSERT(ic_detect_format(webp_magic, 12) == IC_FORMAT_WEBP);
}

TEST(format_detection_bmp) {
    uint8_t bmp_magic[] = { 0x42, 0x4D, 0, 0 };
    ASSERT(ic_detect_format(bmp_magic, 4) == IC_FORMAT_BMP);
}

TEST(format_detection_tiff_le) {
    uint8_t tiff_le[] = { 0x49, 0x49, 0x2A, 0x00 };
    ASSERT(ic_detect_format(tiff_le, 4) == IC_FORMAT_TIFF);
}

TEST(format_detection_tiff_be) {
    uint8_t tiff_be[] = { 0x4D, 0x4D, 0x00, 0x2A };
    ASSERT(ic_detect_format(tiff_be, 4) == IC_FORMAT_TIFF);
}

TEST(format_detection_unknown) {
    uint8_t garbage[] = { 0x00, 0x01, 0x02, 0x03 };
    ASSERT(ic_detect_format(garbage, 4) == IC_FORMAT_UNKNOWN);
}

TEST(jpeg_encode_decode_roundtrip) {
    ICImage src = make_test_image(64, 48);

    /* Encode to JPEG */
    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.95f, 0, 0 };
    int err = ic_encode(&src, IC_FORMAT_JPEG, &opts, &jpeg_data, &jpeg_len);
    ASSERT(err == IC_OK);
    ASSERT(jpeg_data != nullptr);
    ASSERT(jpeg_len > 0);

    /* Verify format detection */
    ASSERT(ic_detect_format(jpeg_data, jpeg_len) == IC_FORMAT_JPEG);

    /* Decode back */
    ICImage decoded;
    err = ic_decode(jpeg_data, jpeg_len, &decoded);
    ASSERT(err == IC_OK);
    ASSERT(decoded.width == 64);
    ASSERT(decoded.height == 48);
    ASSERT(decoded.pixels != nullptr);

    ic_free(jpeg_data);
    ic_image_free(&decoded);
    ic_image_free(&src);
}

TEST(png_encode_decode_roundtrip) {
    ICImage src = make_test_image(32, 32);

    uint8_t* png_data = nullptr;
    size_t png_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    int err = ic_encode(&src, IC_FORMAT_PNG, &opts, &png_data, &png_len);
    ASSERT(err == IC_OK);
    ASSERT(png_data != nullptr);

    /* PNG is lossless — decode should give identical pixels */
    ICImage decoded;
    err = ic_decode(png_data, png_len, &decoded);
    ASSERT(err == IC_OK);
    ASSERT(decoded.width == 32);
    ASSERT(decoded.height == 32);

    /* Verify pixel-perfect round-trip */
    ASSERT(memcmp(src.pixels, decoded.pixels, 32 * 32 * 4) == 0);

    ic_free(png_data);
    ic_image_free(&decoded);
    ic_image_free(&src);
}

TEST(webp_encode_decode_roundtrip) {
    ICImage src = make_test_image(48, 48);

    uint8_t* webp_data = nullptr;
    size_t webp_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    int err = ic_encode(&src, IC_FORMAT_WEBP, &opts, &webp_data, &webp_len);
    ASSERT(err == IC_OK);
    ASSERT(webp_data != nullptr);

    ICImage decoded;
    err = ic_decode(webp_data, webp_len, &decoded);
    ASSERT(err == IC_OK);
    ASSERT(decoded.width == 48);
    ASSERT(decoded.height == 48);

    ic_free(webp_data);
    ic_image_free(&decoded);
    ic_image_free(&src);
}

TEST(webp_lossless_roundtrip) {
    ICImage src = make_test_image(16, 16);

    uint8_t* webp_data = nullptr;
    size_t webp_len = 0;
    ICEncodeOpts opts = { 1.0f, 1, 0 }; /* lossless */
    int err = ic_encode(&src, IC_FORMAT_WEBP, &opts, &webp_data, &webp_len);
    ASSERT(err == IC_OK);

    ICImage decoded;
    err = ic_decode(webp_data, webp_len, &decoded);
    ASSERT(err == IC_OK);

    /* Lossless — pixels must be identical */
    ASSERT(memcmp(src.pixels, decoded.pixels, 16 * 16 * 4) == 0);

    ic_free(webp_data);
    ic_image_free(&decoded);
    ic_image_free(&src);
}

TEST(bmp_encode_decode_roundtrip) {
    ICImage src = make_test_image(40, 30);

    uint8_t* bmp_data = nullptr;
    size_t bmp_len = 0;
    int err = ic_encode(&src, IC_FORMAT_BMP, nullptr, &bmp_data, &bmp_len);
    ASSERT(err == IC_OK);
    ASSERT(bmp_data != nullptr);
    ASSERT(ic_detect_format(bmp_data, bmp_len) == IC_FORMAT_BMP);

    ICImage decoded;
    err = ic_decode(bmp_data, bmp_len, &decoded);
    ASSERT(err == IC_OK);
    ASSERT(decoded.width == 40);
    ASSERT(decoded.height == 30);

    /* BMP 24-bit drops alpha, so compare RGB only */
    for (uint32_t i = 0; i < 40 * 30; i++) {
        ASSERT(src.pixels[i * 4 + 0] == decoded.pixels[i * 4 + 0]); /* R */
        ASSERT(src.pixels[i * 4 + 1] == decoded.pixels[i * 4 + 1]); /* G */
        ASSERT(src.pixels[i * 4 + 2] == decoded.pixels[i * 4 + 2]); /* B */
    }

    ic_free(bmp_data);
    ic_image_free(&decoded);
    ic_image_free(&src);
}

TEST(tiff_encode_decode_roundtrip) {
    ICImage src = make_test_image(24, 24);

    uint8_t* tiff_data = nullptr;
    size_t tiff_len = 0;
    int err = ic_encode(&src, IC_FORMAT_TIFF, nullptr, &tiff_data, &tiff_len);
    ASSERT(err == IC_OK);
    ASSERT(tiff_data != nullptr);
    ASSERT(ic_detect_format(tiff_data, tiff_len) == IC_FORMAT_TIFF);

    ICImage decoded;
    err = ic_decode(tiff_data, tiff_len, &decoded);
    ASSERT(err == IC_OK);
    ASSERT(decoded.width == 24);
    ASSERT(decoded.height == 24);

    ic_free(tiff_data);
    ic_image_free(&decoded);
    ic_image_free(&src);
}

TEST(cross_format_conversion_png_to_jpeg) {
    ICImage src = make_test_image(32, 32);

    /* Encode as PNG first */
    uint8_t* png_data = nullptr;
    size_t png_len = 0;
    int err = ic_encode(&src, IC_FORMAT_PNG, nullptr, &png_data, &png_len);
    ASSERT(err == IC_OK);

    /* Convert PNG → JPEG */
    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    err = ic_convert(png_data, png_len, IC_FORMAT_JPEG, &opts,
                     &jpeg_data, &jpeg_len);
    ASSERT(err == IC_OK);
    ASSERT(ic_detect_format(jpeg_data, jpeg_len) == IC_FORMAT_JPEG);

    ic_free(png_data);
    ic_free(jpeg_data);
    ic_image_free(&src);
}

TEST(cross_format_conversion_jpeg_to_webp) {
    ICImage src = make_test_image(32, 32);

    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    int err = ic_encode(&src, IC_FORMAT_JPEG, &opts, &jpeg_data, &jpeg_len);
    ASSERT(err == IC_OK);

    uint8_t* webp_data = nullptr;
    size_t webp_len = 0;
    err = ic_convert(jpeg_data, jpeg_len, IC_FORMAT_WEBP, &opts,
                     &webp_data, &webp_len);
    ASSERT(err == IC_OK);
    ASSERT(ic_detect_format(webp_data, webp_len) == IC_FORMAT_WEBP);

    ic_free(jpeg_data);
    ic_free(webp_data);
    ic_image_free(&src);
}

TEST(jpeg_lossless_rotate_90) {
    ICImage src = make_test_image(64, 48);

    /* Encode to JPEG */
    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.95f, 0, 0 };
    int err = ic_encode(&src, IC_FORMAT_JPEG, &opts, &jpeg_data, &jpeg_len);
    ASSERT(err == IC_OK);

    /* Lossless rotate 90° */
    uint8_t* rotated = nullptr;
    size_t rotated_len = 0;
    err = ic_jpeg_lossless_rotate(jpeg_data, jpeg_len, IC_ROTATE_90,
                                   &rotated, &rotated_len);
    ASSERT(err == IC_OK);
    ASSERT(rotated != nullptr);
    ASSERT(rotated_len > 0);

    /* Verify dimensions swapped */
    ICImageInfo info;
    err = ic_get_image_info(rotated, rotated_len, &info);
    ASSERT(err == IC_OK);
    ASSERT(info.width == 48); /* was 64x48, now 48x64 */
    ASSERT(info.height == 64);

    /* File size should be similar (lossless = no re-encoding) */
    double size_ratio = (double)rotated_len / jpeg_len;
    ASSERT(size_ratio > 0.9 && size_ratio < 1.1);

    printf("[size ratio: %.3f] ", size_ratio);

    ic_free(jpeg_data);
    ic_free(rotated);
    ic_image_free(&src);
}

TEST(jpeg_binary_exif_strip) {
    ICImage src = make_test_image(32, 32);

    /* Encode to JPEG (no EXIF in our generated images) */
    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.95f, 0, 0 };
    int err = ic_encode(&src, IC_FORMAT_JPEG, &opts, &jpeg_data, &jpeg_len);
    ASSERT(err == IC_OK);

    /* Strip EXIF — should succeed even with no EXIF (output = same or smaller) */
    uint8_t* stripped = nullptr;
    size_t stripped_len = 0;
    err = ic_jpeg_strip_exif(jpeg_data, jpeg_len, &stripped, &stripped_len);
    ASSERT(err == IC_OK);
    ASSERT(stripped != nullptr);
    ASSERT(stripped_len <= jpeg_len);

    /* Output should still be valid JPEG */
    ASSERT(ic_detect_format(stripped, stripped_len) == IC_FORMAT_JPEG);

    ic_free(jpeg_data);
    ic_free(stripped);
    ic_image_free(&src);
}

TEST(get_image_info_jpeg) {
    ICImage src = make_test_image(80, 60);

    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    int err = ic_encode(&src, IC_FORMAT_JPEG, &opts, &jpeg_data, &jpeg_len);
    ASSERT(err == IC_OK);

    ICImageInfo info;
    err = ic_get_image_info(jpeg_data, jpeg_len, &info);
    ASSERT(err == IC_OK);
    ASSERT(info.width == 80);
    ASSERT(info.height == 60);
    ASSERT(info.format == IC_FORMAT_JPEG);
    ASSERT(info.file_size == jpeg_len);

    ic_free(jpeg_data);
    ic_image_free(&src);
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

TEST(avif_encode_decode_roundtrip) {
    ICImage src = make_test_image(32, 32);

    uint8_t* avif_data = nullptr;
    size_t avif_len = 0;
    ICEncodeOpts opts = { 0.8f, 0, 0 };
    int err = ic_encode(&src, IC_FORMAT_AVIF, &opts, &avif_data, &avif_len);
    ASSERT(err == IC_OK);
    ASSERT(avif_data != nullptr);
    ASSERT(avif_len > 0);

    /* Verify format detection */
    ASSERT(ic_detect_format(avif_data, avif_len) == IC_FORMAT_AVIF);

    /* Decode back */
    ICImage decoded;
    err = ic_decode(avif_data, avif_len, &decoded);
    ASSERT(err == IC_OK);
    ASSERT(decoded.width == 32);
    ASSERT(decoded.height == 32);

    ic_free(avif_data);
    ic_image_free(&decoded);
    ic_image_free(&src);
}

/* ══════════════════════════════════════════════════════════════════════ */
/* ── Phase 2: Pixel operations ───────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════ */

TEST(crop_basic) {
    ICImage src = make_test_image(100, 80);

    ICCropRegion region = { 10, 20, 50, 30 };
    ICImage cropped;
    int err = ic_crop(&src, &region, &cropped);
    ASSERT(err == IC_OK);
    ASSERT(cropped.width == 50);
    ASSERT(cropped.height == 30);

    /* Verify pixel at (0,0) of cropped == pixel at (10,20) of source */
    const uint8_t* src_px = src.pixels + 20 * src.stride + 10 * 4;
    const uint8_t* dst_px = cropped.pixels;
    ASSERT(memcmp(src_px, dst_px, 4) == 0);

    /* Verify pixel at (49,29) of cropped == pixel at (59,49) of source */
    src_px = src.pixels + 49 * src.stride + 59 * 4;
    dst_px = cropped.pixels + 29 * cropped.stride + 49 * 4;
    ASSERT(memcmp(src_px, dst_px, 4) == 0);

    ic_image_free(&cropped);
    ic_image_free(&src);
}

TEST(crop_full_image) {
    ICImage src = make_test_image(32, 32);

    ICCropRegion region = { 0, 0, 32, 32 };
    ICImage cropped;
    int err = ic_crop(&src, &region, &cropped);
    ASSERT(err == IC_OK);
    ASSERT(cropped.width == 32);
    ASSERT(cropped.height == 32);

    /* Full crop = identical pixels */
    ASSERT(memcmp(src.pixels, cropped.pixels, 32 * 32 * 4) == 0);

    ic_image_free(&cropped);
    ic_image_free(&src);
}

TEST(crop_invalid_region) {
    ICImage src = make_test_image(32, 32);

    /* Region exceeds image bounds */
    ICCropRegion bad = { 20, 20, 30, 30 }; /* 20+30=50 > 32 */
    ICImage out;
    int err = ic_crop(&src, &bad, &out);
    ASSERT(err == IC_ERROR_INVALID_CROP);

    /* Zero-size region */
    ICCropRegion zero = { 0, 0, 0, 10 };
    err = ic_crop(&src, &zero, &out);
    ASSERT(err == IC_ERROR_INVALID_CROP);

    ic_image_free(&src);
}

TEST(rotate_90_dimensions) {
    ICImage src = make_test_image(80, 60);

    ICImage rotated;
    int err = ic_rotate(&src, IC_ROTATE_90, &rotated);
    ASSERT(err == IC_OK);
    ASSERT(rotated.width == 60);  /* swapped */
    ASSERT(rotated.height == 80);

    ic_image_free(&rotated);
    ic_image_free(&src);
}

TEST(rotate_180_dimensions) {
    ICImage src = make_test_image(80, 60);

    ICImage rotated;
    int err = ic_rotate(&src, IC_ROTATE_180, &rotated);
    ASSERT(err == IC_OK);
    ASSERT(rotated.width == 80);  /* same */
    ASSERT(rotated.height == 60);

    ic_image_free(&rotated);
    ic_image_free(&src);
}

TEST(rotate_270_dimensions) {
    ICImage src = make_test_image(80, 60);

    ICImage rotated;
    int err = ic_rotate(&src, IC_ROTATE_270, &rotated);
    ASSERT(err == IC_OK);
    ASSERT(rotated.width == 60);
    ASSERT(rotated.height == 80);

    ic_image_free(&rotated);
    ic_image_free(&src);
}

TEST(rotate_90_pixel_mapping) {
    ICImage src = make_test_image(4, 3);

    ICImage rotated;
    int err = ic_rotate(&src, IC_ROTATE_90, &rotated);
    ASSERT(err == IC_OK);

    /* After 90° CW: src(0,0) → dst(h-1, 0) i.e. dst(2, 0)
       src pixel at (x=0, y=0) should end up at dst (x=2, y=0) */
    const uint8_t* src_px = src.pixels; /* (0,0) */
    const uint8_t* dst_px = rotated.pixels + 0 * rotated.stride + 2 * 4; /* (2,0) */
    ASSERT(memcmp(src_px, dst_px, 4) == 0);

    /* src(3,0) → dst(2, 3) */
    src_px = src.pixels + 0 * src.stride + 3 * 4;
    dst_px = rotated.pixels + 3 * rotated.stride + 2 * 4;
    ASSERT(memcmp(src_px, dst_px, 4) == 0);

    ic_image_free(&rotated);
    ic_image_free(&src);
}

TEST(rotate_360_identity) {
    ICImage src = make_test_image(16, 16);

    /* 90 + 90 + 90 + 90 = 360 → should return to original */
    ICImage r1, r2, r3, r4;
    ASSERT(ic_rotate(&src, IC_ROTATE_90, &r1) == IC_OK);
    ASSERT(ic_rotate(&r1, IC_ROTATE_90, &r2) == IC_OK);
    ASSERT(ic_rotate(&r2, IC_ROTATE_90, &r3) == IC_OK);
    ASSERT(ic_rotate(&r3, IC_ROTATE_90, &r4) == IC_OK);

    ASSERT(r4.width == src.width);
    ASSERT(r4.height == src.height);
    ASSERT(memcmp(src.pixels, r4.pixels, 16 * 16 * 4) == 0);

    ic_image_free(&r1);
    ic_image_free(&r2);
    ic_image_free(&r3);
    ic_image_free(&r4);
    ic_image_free(&src);
}

TEST(flip_horizontal) {
    ICImage img = make_test_image(8, 4);

    /* Save first row for comparison */
    uint8_t first_pixel[4], last_pixel[4];
    memcpy(first_pixel, img.pixels, 4);
    memcpy(last_pixel, img.pixels + 7 * 4, 4);

    int err = ic_flip_horizontal(&img);
    ASSERT(err == IC_OK);

    /* First pixel should now be last, and vice versa */
    ASSERT(memcmp(img.pixels, last_pixel, 4) == 0);
    ASSERT(memcmp(img.pixels + 7 * 4, first_pixel, 4) == 0);

    /* Flip again → original */
    err = ic_flip_horizontal(&img);
    ASSERT(err == IC_OK);
    ASSERT(memcmp(img.pixels, first_pixel, 4) == 0);

    ic_image_free(&img);
}

TEST(flip_vertical) {
    ICImage img = make_test_image(8, 4);

    /* Save first and last row */
    uint8_t first_row[32], last_row[32]; /* 8 * 4 = 32 bytes */
    memcpy(first_row, img.pixels, 32);
    memcpy(last_row, img.pixels + 3 * img.stride, 32);

    int err = ic_flip_vertical(&img);
    ASSERT(err == IC_OK);

    /* First row should now be last row */
    ASSERT(memcmp(img.pixels, last_row, 32) == 0);
    ASSERT(memcmp(img.pixels + 3 * img.stride, first_row, 32) == 0);

    /* Flip again → original */
    err = ic_flip_vertical(&img);
    ASSERT(err == IC_OK);
    ASSERT(memcmp(img.pixels, first_row, 32) == 0);

    ic_image_free(&img);
}

TEST(resize_nearest_downscale) {
    ICImage src = make_test_image(64, 64);

    ICImage resized;
    int err = ic_resize(&src, 32, 32, IC_FILTER_NEAREST, &resized);
    ASSERT(err == IC_OK);
    ASSERT(resized.width == 32);
    ASSERT(resized.height == 32);
    ASSERT(resized.pixels != nullptr);

    ic_image_free(&resized);
    ic_image_free(&src);
}

TEST(resize_bilinear_upscale) {
    ICImage src = make_test_image(16, 16);

    ICImage resized;
    int err = ic_resize(&src, 48, 48, IC_FILTER_BILINEAR, &resized);
    ASSERT(err == IC_OK);
    ASSERT(resized.width == 48);
    ASSERT(resized.height == 48);

    ic_image_free(&resized);
    ic_image_free(&src);
}

TEST(resize_lanczos_downscale) {
    ICImage src = make_test_image(100, 80);

    ICImage resized;
    int err = ic_resize(&src, 50, 40, IC_FILTER_LANCZOS, &resized);
    ASSERT(err == IC_OK);
    ASSERT(resized.width == 50);
    ASSERT(resized.height == 40);

    ic_image_free(&resized);
    ic_image_free(&src);
}

TEST(resize_to_1x1) {
    ICImage src = make_test_image(64, 64);

    ICImage resized;
    int err = ic_resize(&src, 1, 1, IC_FILTER_BILINEAR, &resized);
    ASSERT(err == IC_OK);
    ASSERT(resized.width == 1);
    ASSERT(resized.height == 1);

    ic_image_free(&resized);
    ic_image_free(&src);
}

TEST(resize_same_size) {
    ICImage src = make_test_image(32, 32);

    ICImage resized;
    int err = ic_resize(&src, 32, 32, IC_FILTER_NEAREST, &resized);
    ASSERT(err == IC_OK);
    ASSERT(resized.width == 32);
    ASSERT(resized.height == 32);

    /* Same size nearest = identical pixels */
    ASSERT(memcmp(src.pixels, resized.pixels, 32 * 32 * 4) == 0);

    ic_image_free(&resized);
    ic_image_free(&src);
}

TEST(crop_then_encode) {
    ICImage src = make_test_image(100, 80);

    /* Crop to 50x40 */
    ICCropRegion region = { 10, 10, 50, 40 };
    ICImage cropped;
    int err = ic_crop(&src, &region, &cropped);
    ASSERT(err == IC_OK);

    /* Encode cropped as PNG */
    uint8_t* png_data = nullptr;
    size_t png_len = 0;
    err = ic_encode(&cropped, IC_FORMAT_PNG, nullptr, &png_data, &png_len);
    ASSERT(err == IC_OK);
    ASSERT(png_data != nullptr);

    /* Decode back and verify dimensions */
    ICImage decoded;
    err = ic_decode(png_data, png_len, &decoded);
    ASSERT(err == IC_OK);
    ASSERT(decoded.width == 50);
    ASSERT(decoded.height == 40);

    ic_free(png_data);
    ic_image_free(&decoded);
    ic_image_free(&cropped);
    ic_image_free(&src);
}

TEST(resize_then_encode) {
    ICImage src = make_test_image(64, 48);

    /* Resize to 32x24 */
    ICImage resized;
    int err = ic_resize(&src, 32, 24, IC_FILTER_LANCZOS, &resized);
    ASSERT(err == IC_OK);

    /* Encode as JPEG */
    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    err = ic_encode(&resized, IC_FORMAT_JPEG, &opts, &jpeg_data, &jpeg_len);
    ASSERT(err == IC_OK);

    /* Decode and verify */
    ICImage decoded;
    err = ic_decode(jpeg_data, jpeg_len, &decoded);
    ASSERT(err == IC_OK);
    ASSERT(decoded.width == 32);
    ASSERT(decoded.height == 24);

    ic_free(jpeg_data);
    ic_image_free(&decoded);
    ic_image_free(&resized);
    ic_image_free(&src);
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main() {
    printf("\n  imagecore test suite\n");
    printf("  ====================\n\n");

    /* Tests are auto-registered via static constructors above */

    printf("\n  Results: %d passed, %d failed\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
