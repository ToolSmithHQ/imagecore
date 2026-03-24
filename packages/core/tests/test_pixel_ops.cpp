#include "test_harness.h"

/* ── Crop ────────────────────────────────────────────────────────────── */

TEST(crop_basic) {
    ICImage src = make_test_image(100, 80);
    ICCropRegion region = { 10, 20, 50, 30 };
    ICImage cropped;
    ASSERT(ic_crop(&src, &region, &cropped) == IC_OK);
    ASSERT(cropped.width == 50 && cropped.height == 30);

    const uint8_t* s = src.pixels + 20 * src.stride + 10 * 4;
    ASSERT(memcmp(s, cropped.pixels, 4) == 0);

    ic_image_free(&cropped); ic_image_free(&src);
}

TEST(crop_full_image) {
    ICImage src = make_test_image(32, 32);
    ICCropRegion region = { 0, 0, 32, 32 };
    ICImage cropped;
    ASSERT(ic_crop(&src, &region, &cropped) == IC_OK);
    ASSERT(memcmp(src.pixels, cropped.pixels, 32 * 32 * 4) == 0);

    ic_image_free(&cropped); ic_image_free(&src);
}

TEST(crop_invalid_region) {
    ICImage src = make_test_image(32, 32);
    ICImage out;
    ICCropRegion bad = { 20, 20, 30, 30 };
    ASSERT(ic_crop(&src, &bad, &out) == IC_ERROR_INVALID_CROP);

    ICCropRegion zero = { 0, 0, 0, 10 };
    ASSERT(ic_crop(&src, &zero, &out) == IC_ERROR_INVALID_CROP);

    ic_image_free(&src);
}

/* ── Rotate ──────────────────────────────────────────────────────────── */

TEST(rotate_90_dimensions) {
    ICImage src = make_test_image(80, 60);
    ICImage out;
    ASSERT(ic_rotate(&src, IC_ROTATE_90, &out) == IC_OK);
    ASSERT(out.width == 60 && out.height == 80);
    ic_image_free(&out); ic_image_free(&src);
}

TEST(rotate_180_dimensions) {
    ICImage src = make_test_image(80, 60);
    ICImage out;
    ASSERT(ic_rotate(&src, IC_ROTATE_180, &out) == IC_OK);
    ASSERT(out.width == 80 && out.height == 60);
    ic_image_free(&out); ic_image_free(&src);
}

TEST(rotate_270_dimensions) {
    ICImage src = make_test_image(80, 60);
    ICImage out;
    ASSERT(ic_rotate(&src, IC_ROTATE_270, &out) == IC_OK);
    ASSERT(out.width == 60 && out.height == 80);
    ic_image_free(&out); ic_image_free(&src);
}

TEST(rotate_90_pixel_mapping) {
    ICImage src = make_test_image(4, 3);
    ICImage out;
    ASSERT(ic_rotate(&src, IC_ROTATE_90, &out) == IC_OK);
    /* src(0,0) → dst(2,0) */
    ASSERT(memcmp(src.pixels, out.pixels + 2 * 4, 4) == 0);
    ic_image_free(&out); ic_image_free(&src);
}

TEST(rotate_360_identity) {
    ICImage src = make_test_image(16, 16);
    ICImage r1, r2, r3, r4;
    ASSERT(ic_rotate(&src, IC_ROTATE_90, &r1) == IC_OK);
    ASSERT(ic_rotate(&r1, IC_ROTATE_90, &r2) == IC_OK);
    ASSERT(ic_rotate(&r2, IC_ROTATE_90, &r3) == IC_OK);
    ASSERT(ic_rotate(&r3, IC_ROTATE_90, &r4) == IC_OK);
    ASSERT(memcmp(src.pixels, r4.pixels, 16 * 16 * 4) == 0);
    ic_image_free(&r1); ic_image_free(&r2);
    ic_image_free(&r3); ic_image_free(&r4); ic_image_free(&src);
}

/* ── Flip ────────────────────────────────────────────────────────────── */

TEST(flip_horizontal) {
    ICImage img = make_test_image(8, 4);
    uint8_t first[4], last[4];
    memcpy(first, img.pixels, 4);
    memcpy(last, img.pixels + 7 * 4, 4);

    ASSERT(ic_flip_horizontal(&img) == IC_OK);
    ASSERT(memcmp(img.pixels, last, 4) == 0);
    ASSERT(memcmp(img.pixels + 7 * 4, first, 4) == 0);

    /* Double flip = identity */
    ASSERT(ic_flip_horizontal(&img) == IC_OK);
    ASSERT(memcmp(img.pixels, first, 4) == 0);
    ic_image_free(&img);
}

TEST(flip_vertical) {
    ICImage img = make_test_image(8, 4);
    uint8_t first_row[32], last_row[32];
    memcpy(first_row, img.pixels, 32);
    memcpy(last_row, img.pixels + 3 * img.stride, 32);

    ASSERT(ic_flip_vertical(&img) == IC_OK);
    ASSERT(memcmp(img.pixels, last_row, 32) == 0);

    ASSERT(ic_flip_vertical(&img) == IC_OK);
    ASSERT(memcmp(img.pixels, first_row, 32) == 0);
    ic_image_free(&img);
}

/* ── Resize ──────────────────────────────────────────────────────────── */

TEST(resize_nearest_downscale) {
    ICImage src = make_test_image(64, 64);
    ICImage out;
    ASSERT(ic_resize(&src, 32, 32, IC_FILTER_NEAREST, &out) == IC_OK);
    ASSERT(out.width == 32 && out.height == 32);
    ic_image_free(&out); ic_image_free(&src);
}

TEST(resize_bilinear_upscale) {
    ICImage src = make_test_image(16, 16);
    ICImage out;
    ASSERT(ic_resize(&src, 48, 48, IC_FILTER_BILINEAR, &out) == IC_OK);
    ASSERT(out.width == 48 && out.height == 48);
    ic_image_free(&out); ic_image_free(&src);
}

TEST(resize_lanczos_downscale) {
    ICImage src = make_test_image(100, 80);
    ICImage out;
    ASSERT(ic_resize(&src, 50, 40, IC_FILTER_LANCZOS, &out) == IC_OK);
    ASSERT(out.width == 50 && out.height == 40);
    ic_image_free(&out); ic_image_free(&src);
}

TEST(resize_to_1x1) {
    ICImage src = make_test_image(64, 64);
    ICImage out;
    ASSERT(ic_resize(&src, 1, 1, IC_FILTER_BILINEAR, &out) == IC_OK);
    ASSERT(out.width == 1 && out.height == 1);
    ic_image_free(&out); ic_image_free(&src);
}

TEST(resize_same_size) {
    ICImage src = make_test_image(32, 32);
    ICImage out;
    ASSERT(ic_resize(&src, 32, 32, IC_FILTER_NEAREST, &out) == IC_OK);
    ASSERT(memcmp(src.pixels, out.pixels, 32 * 32 * 4) == 0);
    ic_image_free(&out); ic_image_free(&src);
}

/* ── Integration: ops + encode ───────────────────────────────────────── */

TEST(crop_then_encode) {
    ICImage src = make_test_image(100, 80);
    ICCropRegion region = { 10, 10, 50, 40 };
    ICImage cropped;
    ASSERT(ic_crop(&src, &region, &cropped) == IC_OK);

    uint8_t* png = nullptr; size_t len = 0;
    ASSERT(ic_encode(&cropped, IC_FORMAT_PNG, nullptr, &png, &len) == IC_OK);

    ICImage decoded;
    ASSERT(ic_decode(png, len, &decoded) == IC_OK);
    ASSERT(decoded.width == 50 && decoded.height == 40);

    ic_free(png); ic_image_free(&decoded);
    ic_image_free(&cropped); ic_image_free(&src);
}

TEST(resize_then_encode) {
    ICImage src = make_test_image(64, 48);
    ICImage resized;
    ASSERT(ic_resize(&src, 32, 24, IC_FILTER_LANCZOS, &resized) == IC_OK);

    uint8_t* jpeg = nullptr; size_t len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    ASSERT(ic_encode(&resized, IC_FORMAT_JPEG, &opts, &jpeg, &len) == IC_OK);

    ICImage decoded;
    ASSERT(ic_decode(jpeg, len, &decoded) == IC_OK);
    ASSERT(decoded.width == 32 && decoded.height == 24);

    ic_free(jpeg); ic_image_free(&decoded);
    ic_image_free(&resized); ic_image_free(&src);
}
