/*
 * ic_encode_with_exif (EXIF embedding per format) and compact TIFF EXIF
 * extraction.
 */

#include "test_harness.h"

#include <vector>

/* ── Helpers ─────────────────────────────────────────────────────────── */

/*
 * Build a realistic EXIF block (TIFF structure) in either byte order:
 * IFD0 { Make="TestCam", Artist="Tester", ExifIFD, GPSInfo },
 * ExifIFD { ExposureTime=1/250 }, GPS { LatitudeRef="N", Latitude=12/1,58/1,1776/100 }.
 * `pad` extra zero bytes are appended (to exercise odd lengths).
 */
static std::vector<uint8_t> make_exif(bool be, size_t pad = 0) {
    std::vector<uint8_t> b;
    auto u16 = [&](uint16_t v) {
        if (be) { b.push_back(v >> 8); b.push_back(v & 0xFF); }
        else    { b.push_back(v & 0xFF); b.push_back(v >> 8); }
    };
    auto u32 = [&](uint32_t v) {
        if (be) { u16(v >> 16); u16(v & 0xFFFF); }
        else    { u16(v & 0xFFFF); u16(v >> 16); }
    };
    auto entry = [&](uint16_t tag, uint16_t type, uint32_t count, uint32_t val) {
        u16(tag); u16(type); u32(count); u32(val);
    };
    /* Layout: header(8) | IFD0 @8: 2+4*12+4 = 54 -> 62 | "TestCam\0" @62 (8)
     * | "Tester\0" @70 (7, +1 pad) | ExifIFD @78: 2+12+4 = 18 -> 96
     * | rational @96 (8) -> 104 | GPS @104: 2+24+4 = 30 -> 134 | 3 rationals @134 (24) -> 158 */
    b.push_back(be ? 'M' : 'I'); b.push_back(be ? 'M' : 'I'); u16(42); u32(8);
    u16(4);
    entry(0x010F, 2, 8, 62);
    entry(0x013B, 2, 7, 70);
    entry(0x8769, 4, 1, 78);
    entry(0x8825, 4, 1, 104);
    u32(0);
    for (char c : std::string("TestCam", 8)) b.push_back((uint8_t)c);
    for (char c : std::string("Tester\0\0", 8)) b.push_back((uint8_t)c);
    u16(1); entry(0x829A, 5, 1, 96); u32(0);
    u32(1); u32(250);
    u16(2);
    /* GPSLatitudeRef "N\0" inline: bytes N,0 then padding */
    u16(0x0001); u16(2); u32(2); b.push_back('N'); b.push_back(0); b.push_back(0); b.push_back(0);
    entry(0x0002, 5, 3, 134);
    u32(0);
    u32(12); u32(1); u32(58); u32(1); u32(1776); u32(100);
    b.insert(b.end(), pad, 0);
    return b;
}

static bool contains(const uint8_t* d, size_t n, const char* s) {
    size_t k = strlen(s);
    for (size_t i = 0; i + k <= n; i++)
        if (!memcmp(d + i, s, k)) return true;
    return false;
}

static bool same_pixels(const ICImage& a, const ICImage& b) {
    if (a.width != b.width || a.height != b.height) return false;
    for (uint32_t y = 0; y < a.height; y++)
        if (memcmp(a.pixels + (size_t)y * a.stride, b.pixels + (size_t)y * b.stride, a.width * 4))
            return false;
    return true;
}

static ICImage make_alpha_image(uint32_t w, uint32_t h) {
    ICImage img = make_test_image(w, h);
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++)
            img.pixels[y * img.stride + x * 4 + 3] = (uint8_t)((x * 7 + y * 3) & 0xFF);
    return img;
}

/*
 * Encode with and without EXIF; check the EXIF reads back byte-identical
 * (expect_identical) and that the decoded pixels match the EXIF-less file.
 */
static bool check_embed(const ICImage& img, ICFormat fmt, int lossless,
                        const std::vector<uint8_t>& exif, bool expect_identical) {
    ICEncodeOpts opts = { 0.85f, lossless, 0 };
    uint8_t *plain = nullptr, *with = nullptr;
    size_t plain_len = 0, with_len = 0;
    if (ic_encode(&img, fmt, &opts, &plain, &plain_len) != IC_OK) return false;
    if (ic_encode_with_exif(&img, fmt, &opts, exif.data(), exif.size(), &with, &with_len) != IC_OK) {
        ic_free(plain);
        return false;
    }
    bool ok = with_len > plain_len;

    uint8_t* got = nullptr; size_t got_len = 0;
    if (ic_get_exif(with, with_len, &got, &got_len) != IC_OK) ok = false;
    else {
        if (expect_identical && (got_len != exif.size() || memcmp(got, exif.data(), got_len)))
            ok = false;
        if (!contains(got, got_len, "TestCam")) ok = false;
        ic_free(got);
    }

    ICImage a, b;
    if (ic_decode(plain, plain_len, &a) != IC_OK) ok = false;
    else {
        if (ic_decode(with, with_len, &b) != IC_OK) ok = false;
        else { ok = ok && same_pixels(a, b); ic_image_free(&b); }
        ic_image_free(&a);
    }
    ic_free(plain);
    ic_free(with);
    return ok;
}

/* ── ic_encode_with_exif: argument handling ──────────────────────────── */

TEST(encode_with_exif_null_matches_encode) {
    ICImage img = make_test_image(20, 20);
    ICEncodeOpts opts = { 0.8f, 0, 0 };
    const ICFormat fmts[] = { IC_FORMAT_JPEG, IC_FORMAT_PNG, IC_FORMAT_WEBP, IC_FORMAT_TIFF };
    for (ICFormat f : fmts) {
        uint8_t *a = nullptr, *b = nullptr; size_t na = 0, nb = 0;
        ASSERT(ic_encode(&img, f, &opts, &a, &na) == IC_OK);
        ASSERT(ic_encode_with_exif(&img, f, &opts, nullptr, 0, &b, &nb) == IC_OK);
        ASSERT(na == nb && memcmp(a, b, na) == 0);
        ic_free(a); ic_free(b);
    }
    ic_image_free(&img);
}

TEST(encode_with_exif_rejects_non_tiff_block) {
    ICImage img = make_test_image(8, 8);
    const uint8_t junk[] = { 'E', 'x', 'i', 'f', 0, 0, 'M', 'M', 0, 42, 0, 0, 0, 8 };
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode_with_exif(&img, IC_FORMAT_JPEG, nullptr, junk, sizeof(junk), &out, &len)
           == IC_ERROR_INVALID_INPUT);
    ic_image_free(&img);
}

/* ── Per-format embedding ────────────────────────────────────────────── */

TEST(encode_with_exif_jpeg) {
    ICImage img = make_test_image(48, 32);
    ASSERT(check_embed(img, IC_FORMAT_JPEG, 0, make_exif(true), true));
    ASSERT(check_embed(img, IC_FORMAT_JPEG, 0, make_exif(false), true));
    ic_image_free(&img);
}

TEST(encode_with_exif_jpeg_app1_after_soi) {
    ICImage img = make_test_image(16, 16);
    auto exif = make_exif(true);
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode_with_exif(&img, IC_FORMAT_JPEG, nullptr, exif.data(), exif.size(), &out, &len) == IC_OK);
    /* SOI, then APPn segments only, the Exif APP1 among them before any other marker. */
    size_t pos = 2;
    bool found = false;
    while (pos + 4 <= len && out[pos] == 0xFF && out[pos + 1] >= 0xE0 && out[pos + 1] <= 0xEF) {
        size_t seg = (out[pos + 2] << 8) | out[pos + 3];
        if (out[pos + 1] == 0xE1 && !memcmp(out + pos + 4, "Exif\0\0", 6)) {
            ASSERT(seg == exif.size() + 8);
            found = true;
        }
        pos += 2 + seg;
    }
    ASSERT(found);
    ic_free(out);
    ic_image_free(&img);
}

TEST(encode_with_exif_jpeg_oversize_skipped) {
    ICImage img = make_test_image(16, 16);
    auto exif = make_exif(true, 65527 - make_exif(true).size() + 1); /* 65528 bytes */
    ASSERT(exif.size() == 65528);
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode_with_exif(&img, IC_FORMAT_JPEG, nullptr, exif.data(), exif.size(), &out, &len) == IC_OK);
    uint8_t* got = nullptr; size_t got_len = 0;
    ASSERT(ic_get_exif(out, len, &got, &got_len) == IC_ERROR_EXIF_NOT_FOUND);
    ic_free(out);

    /* Exactly at the limit still fits. */
    exif.pop_back();
    ASSERT(ic_encode_with_exif(&img, IC_FORMAT_JPEG, nullptr, exif.data(), exif.size(), &out, &len) == IC_OK);
    ASSERT(ic_get_exif(out, len, &got, &got_len) == IC_OK);
    ASSERT(got_len == exif.size() && memcmp(got, exif.data(), got_len) == 0);
    ic_free(got);
    ic_free(out);
    ic_image_free(&img);
}

TEST(encode_with_exif_png) {
    ICImage opaque = make_test_image(48, 32);
    ICImage alpha = make_alpha_image(48, 32);
    ASSERT(check_embed(opaque, IC_FORMAT_PNG, 0, make_exif(true), true));
    ASSERT(check_embed(alpha, IC_FORMAT_PNG, 0, make_exif(false, 1), true));

    /* eXIf must precede IDAT and carry a valid chunk layout. */
    auto exif = make_exif(true);
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode_with_exif(&opaque, IC_FORMAT_PNG, nullptr, exif.data(), exif.size(), &out, &len) == IC_OK);
    size_t pos = 8;
    int exif_at = -1, idat_at = -1, idx = 0;
    while (pos + 12 <= len) {
        uint32_t n = ((uint32_t)out[pos] << 24) | (out[pos + 1] << 16) | (out[pos + 2] << 8) | out[pos + 3];
        if (!memcmp(out + pos + 4, "eXIf", 4) && exif_at < 0) {
            exif_at = idx;
            ASSERT(n == exif.size() && memcmp(out + pos + 8, exif.data(), n) == 0);
        }
        if (!memcmp(out + pos + 4, "IDAT", 4) && idat_at < 0) idat_at = idx;
        pos += 12 + n;
        idx++;
    }
    ASSERT(pos == len);
    ASSERT(exif_at >= 0 && idat_at > exif_at);
    ic_free(out);
    ic_image_free(&opaque);
    ic_image_free(&alpha);
}

/* Walk the RIFF chunks: sizes consistent, VP8X first with the given flags. */
static bool webp_layout_ok(const uint8_t* d, size_t n, uint8_t want_flags,
                           uint32_t w, uint32_t h, size_t exif_len) {
    if (n < 30 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WEBP", 4)) return false;
    uint32_t riff = d[4] | (d[5] << 8) | (d[6] << 16) | ((uint32_t)d[7] << 24);
    if (riff != n - 8) return false;
    if (memcmp(d + 12, "VP8X", 4) || d[20] != want_flags) return false;
    uint32_t cw = (d[24] | (d[25] << 8) | (d[26] << 16)) + 1;
    uint32_t ch = (d[27] | (d[28] << 8) | (d[29] << 16)) + 1;
    if (cw != w || ch != h) return false;
    size_t pos = 12;
    bool exif = false;
    while (pos + 8 <= n) {
        uint32_t c = d[pos + 4] | (d[pos + 5] << 8) | (d[pos + 6] << 16) | ((uint32_t)d[pos + 7] << 24);
        if (!memcmp(d + pos, "EXIF", 4)) exif = (c == exif_len);
        pos += 8 + c + (c & 1);
    }
    return pos == n && exif;
}

TEST(encode_with_exif_webp) {
    ICImage opaque = make_test_image(48, 32);
    ICImage alpha = make_alpha_image(48, 32);
    const auto even = make_exif(true);
    const auto odd = make_exif(false, 1);
    ASSERT(odd.size() & 1);
    ASSERT(check_embed(opaque, IC_FORMAT_WEBP, 0, even, true));  /* VP8 */
    ASSERT(check_embed(opaque, IC_FORMAT_WEBP, 1, odd, true));   /* VP8L */
    ASSERT(check_embed(alpha, IC_FORMAT_WEBP, 0, odd, true));    /* VP8X+ALPH+VP8 */
    ASSERT(check_embed(alpha, IC_FORMAT_WEBP, 1, even, true));   /* VP8L with alpha */

    struct { ICImage* img; int lossless; uint8_t flags; } cases[] = {
        { &opaque, 0, 0x08 }, { &opaque, 1, 0x08 }, { &alpha, 0, 0x18 }, { &alpha, 1, 0x18 },
    };
    for (auto& c : cases) {
        ICEncodeOpts opts = { 0.8f, c.lossless, 0 };
        uint8_t* out = nullptr; size_t len = 0;
        ASSERT(ic_encode_with_exif(c.img, IC_FORMAT_WEBP, &opts, odd.data(), odd.size(), &out, &len) == IC_OK);
        ASSERT(webp_layout_ok(out, len, c.flags, 48, 32, odd.size()));
        ic_free(out);
    }
    ic_image_free(&opaque);
    ic_image_free(&alpha);
}

TEST(encode_with_exif_tiff) {
    ICImage img = make_alpha_image(48, 32);
    /* The TIFF writer merges the block into the file's own IFDs; reading it
     * back rebuilds an equivalent block (IFD layout and byte order are the
     * file's, so not byte-identical to the input). */
    ASSERT(check_embed(img, IC_FORMAT_TIFF, 0, make_exif(false), false));
    ASSERT(check_embed(img, IC_FORMAT_TIFF, 0, make_exif(true), false));

    auto exif = make_exif(true);
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode_with_exif(&img, IC_FORMAT_TIFF, nullptr, exif.data(), exif.size(), &out, &len) == IC_OK);
    ICImageInfo info;
    ASSERT(ic_get_image_info(out, len, &info) == IC_OK && info.has_exif == 1);
    uint8_t* got = nullptr; size_t got_len = 0;
    ASSERT(ic_get_exif(out, len, &got, &got_len) == IC_OK);
    /* Big-endian input, little-endian file: same tags, converted values. */
    ASSERT(got[0] == 'I');
    ASSERT(contains(got, got_len, "TestCam") && contains(got, got_len, "Tester"));
    ic_free(got);
    ic_free(out);
    ic_image_free(&img);
}

TEST(encode_with_exif_avif) {
    ICImage img = make_test_image(48, 32);
    ICEncodeOpts opts = { 0.8f, 0, 0 };
    uint8_t* probe = nullptr; size_t probe_len = 0;
    if (ic_encode(&img, IC_FORMAT_AVIF, &opts, &probe, &probe_len) != IC_OK) {
        ic_image_free(&img); /* AVIF not built on this platform */
        return;
    }
    ic_free(probe);
    ASSERT(check_embed(img, IC_FORMAT_AVIF, 0, make_exif(true), true));
    ASSERT(check_embed(img, IC_FORMAT_AVIF, 0, make_exif(false, 1), true));
    ic_image_free(&img);
}

TEST(encode_with_exif_bmp_ignored) {
    ICImage img = make_test_image(16, 16);
    auto exif = make_exif(true);
    uint8_t *a = nullptr, *b = nullptr; size_t na = 0, nb = 0;
    ASSERT(ic_encode(&img, IC_FORMAT_BMP, nullptr, &a, &na) == IC_OK);
    ASSERT(ic_encode_with_exif(&img, IC_FORMAT_BMP, nullptr, exif.data(), exif.size(), &b, &nb) == IC_OK);
    ASSERT(na == nb && memcmp(a, b, na) == 0);
    ic_free(a); ic_free(b);
    ic_image_free(&img);
}

TEST(tiff_without_exif_reports_none) {
    ICImage img = make_test_image(16, 16);
    uint8_t* out = nullptr; size_t len = 0;
    ASSERT(ic_encode(&img, IC_FORMAT_TIFF, nullptr, &out, &len) == IC_OK);
    uint8_t* got = nullptr; size_t got_len = 0;
    ASSERT(ic_get_exif(out, len, &got, &got_len) == IC_ERROR_EXIF_NOT_FOUND);
    ICImageInfo info;
    ASSERT(ic_get_image_info(out, len, &info) == IC_OK && info.has_exif == 0);
    ic_free(out);
    ic_image_free(&img);
}

/* ── Sample files (real EXIF blocks) ─────────────────────────────────── */

static void sample_exif_to_all_formats(const char* file, bool tiff_identical) {
    size_t len = 0;
    uint8_t* data = load_file(file, &len);
    ASSERT(data);
    uint8_t* exif = nullptr; size_t exif_len = 0;
    ASSERT(ic_get_exif(data, len, &exif, &exif_len) == IC_OK);
    std::vector<uint8_t> block(exif, exif + exif_len);
    ic_free(exif);
    ICImage img;
    ASSERT(ic_decode(data, len, &img) == IC_OK);
    free(data);
    ASSERT(check_embed(img, IC_FORMAT_JPEG, 0, block, true));
    ASSERT(check_embed(img, IC_FORMAT_PNG, 0, block, true));
    ASSERT(check_embed(img, IC_FORMAT_WEBP, 0, block, true));
    ASSERT(check_embed(img, IC_FORMAT_WEBP, 1, block, true));
    ASSERT(check_embed(img, IC_FORMAT_TIFF, 0, block, tiff_identical));
    printf("[exif %zu B] ", block.size());
    ic_image_free(&img);
}

SAMPLE_TEST(sample_exif_jpeg_embed_all) { sample_exif_to_all_formats("exifjpeg.jpg", false); }
SAMPLE_TEST(sample_exif_webp_embed_all) { sample_exif_to_all_formats("exifwebp.webp", false); }
/* A block read from a TIFF is already in the canonical rebuilt layout, so a
 * TIFF -> TIFF round trip reproduces it exactly. */
SAMPLE_TEST(sample_exif_tiff_embed_all) { sample_exif_to_all_formats("exiftiff.tiff", true); }

SAMPLE_TEST(sample_tiff_exif_compact) {
    size_t len = 0;
    uint8_t* data = load_file("exiftiff.tiff", &len);
    ASSERT(data);
    uint8_t* exif = nullptr; size_t exif_len = 0;
    ASSERT(ic_get_exif(data, len, &exif, &exif_len) == IC_OK);
    ASSERT(exif_len < 4096);                       /* no pixel strips */
    ASSERT(contains(exif, exif_len, "TestCam"));
    ASSERT(contains(exif, exif_len, "ImageSmith Test"));
    printf("[%zu -> %zu B] ", len, exif_len);
    ic_free(exif);

    /* After stripping, no private fields remain. */
    uint8_t* stripped = nullptr; size_t stripped_len = 0;
    ASSERT(ic_strip_exif(data, len, &stripped, &stripped_len) == IC_OK);
    int rc = ic_get_exif(stripped, stripped_len, &exif, &exif_len);
    ASSERT(rc == IC_ERROR_EXIF_NOT_FOUND);
    ASSERT(!contains(stripped, stripped_len, "TestCam"));
    ic_free(stripped);
    free(data);
}
