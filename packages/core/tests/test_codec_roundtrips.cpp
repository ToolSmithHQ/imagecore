#include "test_harness.h"

TEST(jpeg_encode_decode_roundtrip) {
    ICImage src = make_test_image(64, 48);
    uint8_t* data = nullptr; size_t len = 0;
    ICEncodeOpts opts = { 0.95f, 0, 0 };
    ASSERT(ic_encode(&src, IC_FORMAT_JPEG, &opts, &data, &len) == IC_OK);
    ASSERT(ic_detect_format(data, len) == IC_FORMAT_JPEG);

    ICImage decoded;
    ASSERT(ic_decode(data, len, &decoded) == IC_OK);
    ASSERT(decoded.width == 64 && decoded.height == 48);

    ic_free(data); ic_image_free(&decoded); ic_image_free(&src);
}

TEST(png_encode_decode_roundtrip) {
    ICImage src = make_test_image(32, 32);
    uint8_t* data = nullptr; size_t len = 0;
    ASSERT(ic_encode(&src, IC_FORMAT_PNG, nullptr, &data, &len) == IC_OK);

    ICImage decoded;
    ASSERT(ic_decode(data, len, &decoded) == IC_OK);
    ASSERT(decoded.width == 32 && decoded.height == 32);
    /* PNG is lossless — pixel-perfect round-trip */
    ASSERT(memcmp(src.pixels, decoded.pixels, 32 * 32 * 4) == 0);

    ic_free(data); ic_image_free(&decoded); ic_image_free(&src);
}

TEST(webp_encode_decode_roundtrip) {
    ICImage src = make_test_image(48, 48);
    uint8_t* data = nullptr; size_t len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    ASSERT(ic_encode(&src, IC_FORMAT_WEBP, &opts, &data, &len) == IC_OK);

    ICImage decoded;
    ASSERT(ic_decode(data, len, &decoded) == IC_OK);
    ASSERT(decoded.width == 48 && decoded.height == 48);

    ic_free(data); ic_image_free(&decoded); ic_image_free(&src);
}

TEST(webp_lossless_roundtrip) {
    ICImage src = make_test_image(16, 16);
    uint8_t* data = nullptr; size_t len = 0;
    ICEncodeOpts opts = { 1.0f, 1, 0 };
    ASSERT(ic_encode(&src, IC_FORMAT_WEBP, &opts, &data, &len) == IC_OK);

    ICImage decoded;
    ASSERT(ic_decode(data, len, &decoded) == IC_OK);
    ASSERT(memcmp(src.pixels, decoded.pixels, 16 * 16 * 4) == 0);

    ic_free(data); ic_image_free(&decoded); ic_image_free(&src);
}

TEST(bmp_encode_decode_roundtrip) {
    ICImage src = make_test_image(40, 30);
    uint8_t* data = nullptr; size_t len = 0;
    ASSERT(ic_encode(&src, IC_FORMAT_BMP, nullptr, &data, &len) == IC_OK);
    ASSERT(ic_detect_format(data, len) == IC_FORMAT_BMP);

    ICImage decoded;
    ASSERT(ic_decode(data, len, &decoded) == IC_OK);
    ASSERT(decoded.width == 40 && decoded.height == 30);
    /* BMP 24-bit drops alpha — compare RGB only */
    for (uint32_t i = 0; i < 40 * 30; i++) {
        ASSERT(src.pixels[i*4+0] == decoded.pixels[i*4+0]);
        ASSERT(src.pixels[i*4+1] == decoded.pixels[i*4+1]);
        ASSERT(src.pixels[i*4+2] == decoded.pixels[i*4+2]);
    }

    ic_free(data); ic_image_free(&decoded); ic_image_free(&src);
}

TEST(tiff_encode_decode_roundtrip) {
    ICImage src = make_test_image(24, 24);
    uint8_t* data = nullptr; size_t len = 0;
    ASSERT(ic_encode(&src, IC_FORMAT_TIFF, nullptr, &data, &len) == IC_OK);
    ASSERT(ic_detect_format(data, len) == IC_FORMAT_TIFF);

    ICImage decoded;
    ASSERT(ic_decode(data, len, &decoded) == IC_OK);
    ASSERT(decoded.width == 24 && decoded.height == 24);

    ic_free(data); ic_image_free(&decoded); ic_image_free(&src);
}

TEST(avif_encode_decode_roundtrip) {
    ICImage src = make_test_image(32, 32);
    uint8_t* data = nullptr; size_t len = 0;
    ICEncodeOpts opts = { 0.8f, 0, 0 };
    ASSERT(ic_encode(&src, IC_FORMAT_AVIF, &opts, &data, &len) == IC_OK);
    ASSERT(ic_detect_format(data, len) == IC_FORMAT_AVIF);

    ICImage decoded;
    ASSERT(ic_decode(data, len, &decoded) == IC_OK);
    ASSERT(decoded.width == 32 && decoded.height == 32);

    ic_free(data); ic_image_free(&decoded); ic_image_free(&src);
}

TEST(cross_format_png_to_jpeg) {
    ICImage src = make_test_image(32, 32);
    uint8_t* png = nullptr; size_t png_len = 0;
    ASSERT(ic_encode(&src, IC_FORMAT_PNG, nullptr, &png, &png_len) == IC_OK);

    uint8_t* jpeg = nullptr; size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    ASSERT(ic_convert(png, png_len, IC_FORMAT_JPEG, &opts, &jpeg, &jpeg_len) == IC_OK);
    ASSERT(ic_detect_format(jpeg, jpeg_len) == IC_FORMAT_JPEG);

    ic_free(png); ic_free(jpeg); ic_image_free(&src);
}

TEST(cross_format_jpeg_to_webp) {
    ICImage src = make_test_image(32, 32);
    uint8_t* jpeg = nullptr; size_t jpeg_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    ASSERT(ic_encode(&src, IC_FORMAT_JPEG, &opts, &jpeg, &jpeg_len) == IC_OK);

    uint8_t* webp = nullptr; size_t webp_len = 0;
    ASSERT(ic_convert(jpeg, jpeg_len, IC_FORMAT_WEBP, &opts, &webp, &webp_len) == IC_OK);
    ASSERT(ic_detect_format(webp, webp_len) == IC_FORMAT_WEBP);

    ic_free(jpeg); ic_free(webp); ic_image_free(&src);
}

/* ── PNG: RGB when opaque, RGBA otherwise; always lossless ───────────── */

static bool rt_same_pixels(const ICImage& a, const ICImage& b) {
    if (a.width != b.width || a.height != b.height) return false;
    for (uint32_t y = 0; y < a.height; y++)
        if (memcmp(a.pixels + (size_t)y * a.stride, b.pixels + (size_t)y * b.stride, a.width * 4))
            return false;
    return true;
}

static ICImage rt_alpha_image(uint32_t w, uint32_t h) {
    ICImage img = make_test_image(w, h);
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++)
            img.pixels[y * img.stride + x * 4 + 3] = (uint8_t)((x * 7 + y * 3) & 0xFF);
    return img;
}

TEST(png_opaque_writes_rgb_lossless) {
    ICImage img = make_test_image(37, 23);
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode(&img, IC_FORMAT_PNG, nullptr, &out, &len) == IC_OK);
    ASSERT(len > 26 && out[24] == 8 && out[25] == 2); /* IHDR: depth 8, colour type RGB */
    ICImage back;
    ASSERT(ic_decode(out, len, &back) == IC_OK);
    ASSERT(rt_same_pixels(img, back));
    ic_image_free(&back);
    ic_free(out);
    ic_image_free(&img);
}

TEST(png_transparent_writes_rgba_lossless) {
    ICImage img = rt_alpha_image(37, 23);
    img.pixels[3] = 0;   /* fully transparent pixel keeps its RGB */
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode(&img, IC_FORMAT_PNG, nullptr, &out, &len) == IC_OK);
    ASSERT(len > 26 && out[24] == 8 && out[25] == 6); /* RGBA */
    ICImage back;
    ASSERT(ic_decode(out, len, &back) == IC_OK);
    ASSERT(rt_same_pixels(img, back));
    ic_image_free(&back);
    ic_free(out);
    ic_image_free(&img);
}

TEST(png_single_translucent_pixel_keeps_alpha) {
    ICImage img = make_test_image(16, 16);
    img.pixels[(15 * 16 + 15) * 4 + 3] = 254;
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode(&img, IC_FORMAT_PNG, nullptr, &out, &len) == IC_OK);
    ASSERT(out[25] == 6);
    ICImage back;
    ASSERT(ic_decode(out, len, &back) == IC_OK);
    ASSERT(rt_same_pixels(img, back));
    ic_image_free(&back);
    ic_free(out);
    ic_image_free(&img);
}

TEST(png_padded_stride_roundtrip) {
    ICImage src = make_test_image(10, 7);
    ICImage img = src;
    img.stride = 10 * 4 + 12;
    img.pixels = static_cast<uint8_t*>(calloc(img.stride * 7, 1));
    for (uint32_t y = 0; y < 7; y++) memcpy(img.pixels + y * img.stride, src.pixels + y * src.stride, 40);
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode(&img, IC_FORMAT_PNG, nullptr, &out, &len) == IC_OK);
    ICImage back;
    ASSERT(ic_decode(out, len, &back) == IC_OK);
    ASSERT(rt_same_pixels(src, back));
    ic_image_free(&back);
    ic_free(out);
    free(img.pixels);
    ic_image_free(&src);
}
