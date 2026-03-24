/*
 * Tests using real sample image files.
 * These tests are deferred (SAMPLE_TEST macro) and only run
 * when a samples directory is passed as a command-line argument.
 */

#include "test_harness.h"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static void test_decode(const char* filename, ICFormat expected_fmt, uint32_t min_w, uint32_t min_h) {
    size_t len = 0;
    uint8_t* data = load_file(filename, &len);
    if (!data) { printf("[SKIP: %s not found] ", filename); return; }

    ASSERT(ic_detect_format(data, len) == expected_fmt);

    ICImageInfo info;
    ASSERT(ic_get_image_info(data, len, &info) == IC_OK);
    ASSERT(info.width >= min_w && info.height >= min_h);

    ICImage img;
    ASSERT(ic_decode(data, len, &img) == IC_OK);
    ASSERT(img.pixels != nullptr);
    printf("[%ux%u] ", img.width, img.height);

    ic_image_free(&img);
    free(data);
}

static void test_convert(const char* filename, ICFormat target) {
    size_t len = 0;
    uint8_t* data = load_file(filename, &len);
    if (!data) { printf("[SKIP] "); return; }

    ICEncodeOpts opts = { 0.9f, 0, 0 };
    uint8_t* out = nullptr; size_t out_len = 0;
    ASSERT(ic_convert(data, len, target, &opts, &out, &out_len) == IC_OK);
    ASSERT(ic_detect_format(out, out_len) == target);

    ICImage decoded;
    ASSERT(ic_decode(out, out_len, &decoded) == IC_OK);
    printf("[%zu->%zu bytes] ", len, out_len);

    ic_image_free(&decoded); ic_free(out); free(data);
}

static void test_operations(const char* filename) {
    size_t len = 0;
    uint8_t* data = load_file(filename, &len);
    if (!data) { printf("[SKIP] "); return; }

    ICImage img;
    ASSERT(ic_decode(data, len, &img) == IC_OK);
    free(data);

    uint32_t w = img.width, h = img.height;

    ICCropRegion region = { w/4, h/4, w/2, h/2 };
    ICImage cropped;
    ASSERT(ic_crop(&img, &region, &cropped) == IC_OK);
    ASSERT(cropped.width == w/2);
    ic_image_free(&cropped);

    ICImage resized;
    ASSERT(ic_resize(&img, 100, 100, IC_FILTER_LANCZOS, &resized) == IC_OK);
    ic_image_free(&resized);

    ICImage rotated;
    ASSERT(ic_rotate(&img, IC_ROTATE_90, &rotated) == IC_OK);
    ASSERT(rotated.width == h && rotated.height == w);
    ic_image_free(&rotated);

    size_t total = (size_t)img.stride * img.height;
    ICImage flipped;
    flipped.width = img.width; flipped.height = img.height;
    flipped.stride = img.stride;
    flipped.pixels = static_cast<uint8_t*>(malloc(total));
    memcpy(flipped.pixels, img.pixels, total);
    ASSERT(ic_flip_horizontal(&flipped) == IC_OK);
    ic_image_free(&flipped);

    printf("[crop+resize+rotate+flip OK] ");
    ic_image_free(&img);
}

/* ── Decode tests ────────────────────────────────────────────────────── */

SAMPLE_TEST(sample_jpeg_decode) { test_decode("samplejpeg.jpeg", IC_FORMAT_JPEG, 100, 100); }
SAMPLE_TEST(sample_png_decode) { test_decode("samplepngwithtransparency.png", IC_FORMAT_PNG, 100, 100); }
SAMPLE_TEST(sample_bmp_decode) { test_decode("samplebmp.bmp", IC_FORMAT_BMP, 100, 100); }
SAMPLE_TEST(sample_tiff_decode) { test_decode("sampletiff.tiff", IC_FORMAT_TIFF, 100, 100); }
SAMPLE_TEST(sample_webp_lossy_decode) { test_decode("webplossy.webp", IC_FORMAT_WEBP, 100, 100); }
SAMPLE_TEST(sample_webp_lossless_decode) { test_decode("webplossless.webp", IC_FORMAT_WEBP, 100, 100); }

SAMPLE_TEST(sample_avif_decode) {
    size_t len = 0;
    uint8_t* data = load_file("sampleavif.avif", &len);
    if (!data) { printf("[SKIP: not found] "); return; }
    ASSERT(ic_detect_format(data, len) == IC_FORMAT_AVIF);
    ICImage img;
    int err = ic_decode(data, len, &img);
    if (err != IC_OK) { printf("[SKIP: AVIF not available] "); free(data); return; }
    printf("[%ux%u] ", img.width, img.height);
    ic_image_free(&img); free(data);
}

/* ── Conversion tests ────────────────────────────────────────────────── */

SAMPLE_TEST(sample_jpeg_to_png) { test_convert("samplejpeg.jpeg", IC_FORMAT_PNG); }
SAMPLE_TEST(sample_jpeg_to_webp) { test_convert("samplejpeg.jpeg", IC_FORMAT_WEBP); }
SAMPLE_TEST(sample_jpeg_to_bmp) { test_convert("samplejpeg.jpeg", IC_FORMAT_BMP); }
SAMPLE_TEST(sample_jpeg_to_tiff) { test_convert("samplejpeg.jpeg", IC_FORMAT_TIFF); }
SAMPLE_TEST(sample_jpeg_to_avif) { test_convert("samplejpeg.jpeg", IC_FORMAT_AVIF); }
SAMPLE_TEST(sample_png_to_jpeg) { test_convert("samplepngwithtransparency.png", IC_FORMAT_JPEG); }
SAMPLE_TEST(sample_png_to_webp) { test_convert("samplepngwithtransparency.png", IC_FORMAT_WEBP); }
SAMPLE_TEST(sample_bmp_to_png) { test_convert("samplebmp.bmp", IC_FORMAT_PNG); }
SAMPLE_TEST(sample_tiff_to_jpeg) { test_convert("sampletiff.tiff", IC_FORMAT_JPEG); }
SAMPLE_TEST(sample_webp_to_png) { test_convert("webplossy.webp", IC_FORMAT_PNG); }

SAMPLE_TEST(sample_avif_to_jpeg) {
    size_t len = 0;
    uint8_t* data = load_file("sampleavif.avif", &len);
    if (!data) { printf("[SKIP] "); return; }
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    uint8_t* out = nullptr; size_t out_len = 0;
    int err = ic_convert(data, len, IC_FORMAT_JPEG, &opts, &out, &out_len);
    if (err != IC_OK) { printf("[SKIP: AVIF not available] "); free(data); return; }
    printf("[%zu->%zu bytes] ", len, out_len);
    ic_free(out); free(data);
}

SAMPLE_TEST(sample_avif_to_png) {
    size_t len = 0;
    uint8_t* data = load_file("sampleavif.avif", &len);
    if (!data) { printf("[SKIP] "); return; }
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    uint8_t* out = nullptr; size_t out_len = 0;
    int err = ic_convert(data, len, IC_FORMAT_PNG, &opts, &out, &out_len);
    if (err != IC_OK) { printf("[SKIP: AVIF not available] "); free(data); return; }
    printf("[%zu->%zu bytes] ", len, out_len);
    ic_free(out); free(data);
}

/* ── Operation tests ─────────────────────────────────────────────────── */

SAMPLE_TEST(sample_jpeg_operations) { test_operations("samplejpeg.jpeg"); }
SAMPLE_TEST(sample_png_operations) { test_operations("samplepngwithtransparency.png"); }
SAMPLE_TEST(sample_bmp_operations) { test_operations("samplebmp.bmp"); }
SAMPLE_TEST(sample_webp_operations) { test_operations("webplossy.webp"); }

/* ── Lossless JPEG tests ─────────────────────────────────────────────── */

SAMPLE_TEST(sample_jpeg_lossless_rotate) {
    size_t len = 0;
    uint8_t* data = load_file("samplejpeg.jpeg", &len);
    if (!data) { printf("[SKIP] "); return; }

    uint8_t* rotated = nullptr; size_t rotated_len = 0;
    ASSERT(ic_jpeg_lossless_rotate(data, len, IC_ROTATE_90, &rotated, &rotated_len) == IC_OK);

    ICImageInfo info, orig;
    ic_get_image_info(rotated, rotated_len, &info);
    ic_get_image_info(data, len, &orig);
    /* MCU trimming may adjust by up to 16px */
    ASSERT(info.width >= orig.height - 16 && info.width <= orig.height);
    ASSERT(info.height >= orig.width - 16 && info.height <= orig.width);
    printf("[%ux%u->%ux%u] ", orig.width, orig.height, info.width, info.height);

    double ratio = (double)rotated_len / len;
    printf("[ratio: %.3f] ", ratio);
    ASSERT(ratio > 0.8 && ratio < 1.2);

    ic_free(rotated); free(data);
}

SAMPLE_TEST(sample_jpeg_exif_strip) {
    size_t len = 0;
    uint8_t* data = load_file("samplejpeg.jpeg", &len);
    if (!data) { printf("[SKIP] "); return; }

    uint8_t* stripped = nullptr; size_t stripped_len = 0;
    ASSERT(ic_jpeg_strip_exif(data, len, &stripped, &stripped_len) == IC_OK);
    ASSERT(stripped_len <= len);
    printf("[%zu->%zu bytes] ", len, stripped_len);

    ic_free(stripped); free(data);
}
