/*
 * Orientation tests (samples/orientation/).
 *
 * Fixtures: a 120x80 photo stored as-is, tagged with each orientation:
 *   jpeg_oN.jpg   exiftool -n -Orientation=N
 *   png_oN.png    exiftool -n -Orientation=N         (eXIf chunk)
 *   webp_oN.webp  exiftool -n -Orientation=N         (lossless VP8L + EXIF)
 *   tiff_oN.tif   exiftool -n -IFD0:Orientation#=N   (TIFF's own tag)
 *   webplossy_o6.webp                                (lossy VP8 + EXIF, o=6)
 *   avif_<irot/imir>.avif  libavif with irot/imir properties
 * References: ref_oN.png = the png_oN.png fixture as displayed by macOS
 * ImageIO (CGImageSourceCreateThumbnailAtIndex with
 * kCGImageSourceCreateThumbnailWithTransform), written without metadata.
 * ImageIO reads the AVIF fixtures as EXIF orientations
 *   none 1, imir1 2, irot2 3, imir0 4, irot1_imir0 5, irot3 6, irot1_imir1 7, irot1 8.
 */

#include "test_harness.h"

#include <cmath>
#include <string>

namespace {

struct Pix {
    ICImage img{};
    bool ok = false;
    ~Pix() { ic_image_free(&img); }
};

struct Buf {
    uint8_t* data = nullptr;
    size_t len = 0;
    ~Buf() { free(data); }
};

bool load(const std::string& name, Buf& b) {
    std::string path = "orientation/" + name;
    b.data = load_file(path.c_str(), &b.len);
    return b.data != nullptr;
}

bool decode_buf(const uint8_t* d, size_t n, Pix& p) {
    p.ok = ic_decode(d, n, &p.img) == IC_OK;
    return p.ok;
}

bool decode_file(const std::string& name, Pix& p) {
    Buf b;
    return load(name, b) && decode_buf(b.data, b.len, p);
}

/* Mean absolute difference over RGB of a's sub-rectangle at (ox, oy) vs b. */
double mean_diff(const ICImage& a, uint32_t ox, uint32_t oy, const ICImage& b) {
    if (ox + b.width > a.width || oy + b.height > a.height) return 1e9;
    double sum = 0;
    for (uint32_t y = 0; y < b.height; y++)
        for (uint32_t x = 0; x < b.width; x++) {
            const uint8_t* pa = a.pixels + (size_t)(y + oy) * a.stride + (size_t)(x + ox) * 4;
            const uint8_t* pb = b.pixels + (size_t)y * b.stride + (size_t)x * 4;
            for (int c = 0; c < 3; c++) sum += std::abs((int)pa[c] - (int)pb[c]);
        }
    return sum / ((double)b.width * b.height * 3);
}

double diff(const ICImage& a, const ICImage& b) {
    if (a.width != b.width || a.height != b.height) return 1e9;
    return mean_diff(a, 0, 0, b);
}

/* Best match of `part` inside `whole` over all offsets up to max_off. */
double best_sub_diff(const ICImage& whole, const ICImage& part, uint32_t max_off,
                     uint32_t* bx = nullptr, uint32_t* by = nullptr) {
    double best = 1e9;
    for (uint32_t oy = 0; oy <= max_off; oy++)
        for (uint32_t ox = 0; ox <= max_off; ox++) {
            double d = mean_diff(whole, ox, oy, part);
            if (d < best) { best = d; if (bx) *bx = ox; if (by) *by = oy; }
        }
    return best;
}

int exif_orientation(const uint8_t* d, size_t n) {
    uint8_t* e = nullptr; size_t el = 0;
    if (ic_get_exif(d, n, &e, &el) != IC_OK) return 0; /* no EXIF */
    bool le = e[0] == 'I';
    auto u16 = [&](size_t o) { return le ? (e[o] | (e[o + 1] << 8)) : ((e[o] << 8) | e[o + 1]); };
    auto u32 = [&](size_t o) {
        return le ? (uint32_t)(e[o] | (e[o + 1] << 8) | (e[o + 2] << 16) | ((uint32_t)e[o + 3] << 24))
                  : (uint32_t)(((uint32_t)e[o] << 24) | (e[o + 1] << 16) | (e[o + 2] << 8) | e[o + 3]);
    };
    int o = 1;
    uint32_t ifd = u32(4);
    int cnt = u16(ifd);
    for (int i = 0; i < cnt; i++)
        if (u16(ifd + 2 + i * 12) == 0x0112) o = u16(ifd + 2 + i * 12 + 8);
    ic_free(e);
    return o;
}

const char* kFormats[] = { "jpeg", "png", "webp", "tiff" };
const char* kExt[] = { "jpg", "png", "webp", "tif" };

std::string fixture(int f, int n) {
    return std::string(kFormats[f]) + "_o" + std::to_string(n) + "." + kExt[f];
}

std::string ref(int n) { return "ref_o" + std::to_string(n) + ".png"; }

/* The ImageIO reference vs a lossless decode is within rounding; JPEG goes
 * through two different IDCT/upsampling implementations. */
const double kExactTol = 1.0;
const double kJpegTol = 3.0;

} /* namespace */

/* ── Decode + info, all 8 orientations ───────────────────────────────── */

SAMPLE_TEST(orientation_decode_and_info_all_formats) {
    Buf probe;
    if (!load("ref_o1.png", probe)) { printf("[SKIP: no orientation fixtures] "); return; }
    double worst[4] = {0, 0, 0, 0};
    for (int n = 1; n <= 8; n++) {
        Pix r;
        ASSERT(decode_file(ref(n), r));
        uint32_t dw = n >= 5 ? 80 : 120, dh = n >= 5 ? 120 : 80;
        ASSERT(r.img.width == dw && r.img.height == dh);
        for (int f = 0; f < 4; f++) {
            Buf b;
            ASSERT(load(fixture(f, n), b));
            ICImageInfo info;
            ASSERT(ic_get_image_info(b.data, b.len, &info) == IC_OK);
            ASSERT(info.width == dw && info.height == dh);
            Pix p;
            ASSERT(decode_buf(b.data, b.len, p));
            double d = diff(p.img, r.img);
            if (d > worst[f]) worst[f] = d;
            if (d > (f == 0 ? kJpegTol : kExactTol))
                printf("\n    %s: diff %.3f ", fixture(f, n).c_str(), d);
            ASSERT(d <= (f == 0 ? kJpegTol : kExactTol));
        }
    }
    printf("[max diff jpeg %.2f png %.2f webp %.2f tiff %.2f] ",
           worst[0], worst[1], worst[2], worst[3]);
}

SAMPLE_TEST(orientation_lossy_webp_decode) {
    Pix p, r;
    if (!decode_file("webplossy_o6.webp", p)) { printf("[SKIP] "); return; }
    ASSERT(decode_file(ref(6), r));
    double d = diff(p.img, r.img);
    printf("[diff %.2f] ", d);
    ASSERT(d <= 4.0);
}

/* ── AVIF irot / imir ────────────────────────────────────────────────── */

SAMPLE_TEST(orientation_avif_irot_imir) {
    struct { const char* name; int o; } cases[] = {
        {"none", 1}, {"imir1", 2}, {"irot2", 3}, {"imir0", 4},
        {"irot1_imir0", 5}, {"irot3", 6}, {"irot1_imir1", 7}, {"irot1", 8},
    };
    Buf probe;
    if (!load("avif_none.avif", probe)) { printf("[SKIP] "); return; }
    ICImageInfo pi;
    if (ic_get_image_info(probe.data, probe.len, &pi) != IC_OK) { printf("[SKIP: no AVIF] "); return; }
    double worst = 0;
    for (auto& c : cases) {
        Buf b;
        ASSERT(load(std::string("avif_") + c.name + ".avif", b));
        ICImageInfo info;
        ASSERT(ic_get_image_info(b.data, b.len, &info) == IC_OK);
        uint32_t dw = c.o >= 5 ? 80 : 120, dh = c.o >= 5 ? 120 : 80;
        ASSERT(info.width == dw && info.height == dh);
        Pix p, r;
        ASSERT(decode_buf(b.data, b.len, p));
        ASSERT(decode_file(ref(c.o), r));
        double d = diff(p.img, r.img);
        if (d > worst) worst = d;
        if (d > 4.0) printf("\n    avif_%s: diff %.3f ", c.name, d);
        ASSERT(d <= 4.0);
    }
    printf("[max diff %.2f] ", worst);
}

SAMPLE_TEST(orientation_avif_encode_ignores_exif_orientation) {
    Buf b;
    if (!load("jpeg_o6.jpg", b)) { printf("[SKIP] "); return; }
    ICImage small = make_test_image(40, 24);
    uint8_t* exif = nullptr; size_t el = 0;
    ASSERT(ic_get_exif(b.data, b.len, &exif, &el) == IC_OK);
    uint8_t* out = nullptr; size_t ol = 0;
    int rc = ic_encode_with_exif(&small, IC_FORMAT_AVIF, nullptr, exif, el, &out, &ol);
    if (rc == IC_ERROR_UNSUPPORTED_FORMAT) { printf("[SKIP: no AVIF] "); ic_free(exif); ic_image_free(&small); return; }
    ASSERT(rc == IC_OK);
    /* EXIF stored verbatim, but no irot: still 40x24 as displayed. */
    ICImageInfo info;
    ASSERT(ic_get_image_info(out, ol, &info) == IC_OK);
    ASSERT(info.width == 40 && info.height == 24);
    uint8_t* back = nullptr; size_t bl = 0;
    ASSERT(ic_get_exif(out, ol, &back, &bl) == IC_OK);
    ASSERT(bl == el && memcmp(back, exif, el) == 0);
    ic_free(back); ic_free(out); ic_free(exif); ic_image_free(&small);
}

/* ── Convert / strip ─────────────────────────────────────────────────── */

SAMPLE_TEST(orientation_convert_is_upright) {
    for (int n = 1; n <= 8; n++) {
        Buf b;
        if (!load(fixture(0, n), b)) { printf("[SKIP] "); return; }
        uint8_t* out = nullptr; size_t ol = 0;
        ICEncodeOpts opts = { 0.9f, 0, 0 };
        ASSERT(ic_convert(b.data, b.len, IC_FORMAT_PNG, &opts, &out, &ol) == IC_OK);
        ASSERT(exif_orientation(out, ol) == 0); /* no metadata */
        Pix p, r, src;
        ASSERT(decode_buf(out, ol, p));
        ASSERT(decode_buf(b.data, b.len, src));
        ASSERT(decode_file(ref(n), r));
        ASSERT(diff(p.img, src.img) == 0.0);   /* PNG: exact decoded pixels */
        ASSERT(diff(p.img, r.img) <= kJpegTol);
        ic_free(out);
    }
}

SAMPLE_TEST(orientation_jpeg_strip_keeps_only_orientation) {
    for (int n = 1; n <= 8; n++) {
        Buf b;
        if (!load(fixture(0, n), b)) { printf("[SKIP] "); return; }
        for (int generic = 0; generic < 2; generic++) {
            uint8_t* out = nullptr; size_t ol = 0;
            ASSERT((generic ? ic_strip_exif(b.data, b.len, &out, &ol)
                            : ic_jpeg_strip_exif(b.data, b.len, &out, &ol)) == IC_OK);
            ASSERT(ol <= b.len);
            uint8_t* e = nullptr; size_t el = 0;
            int rc = ic_get_exif(out, ol, &e, &el);
            if (n == 1) {
                ASSERT(rc == IC_ERROR_EXIF_NOT_FOUND);
            } else {
                ASSERT(rc == IC_OK && el == 26);  /* header + 1-entry IFD0 only */
                ic_free(e);
                ASSERT(exif_orientation(out, ol) == n);
            }
            Pix p, src;
            ASSERT(decode_buf(out, ol, p));
            ASSERT(decode_buf(b.data, b.len, src));
            ASSERT(diff(p.img, src.img) == 0.0); /* same entropy data, same view */
            ic_free(out);
        }
    }
}

SAMPLE_TEST(orientation_strip_png_webp_tiff_upright) {
    for (int f = 1; f < 4; f++) {
        for (int n = 1; n <= 8; n++) {
            Buf b;
            if (!load(fixture(f, n), b)) { printf("[SKIP] "); return; }
            uint8_t* out = nullptr; size_t ol = 0;
            ASSERT(ic_strip_exif(b.data, b.len, &out, &ol) == IC_OK);
            ICImageInfo info;
            ASSERT(ic_get_image_info(out, ol, &info) == IC_OK);
            ASSERT(info.width == (n >= 5 ? 80u : 120u));
            if (f != 3) {
                uint8_t* e = nullptr; size_t el = 0;
                ASSERT(ic_get_exif(out, ol, &e, &el) == IC_ERROR_EXIF_NOT_FOUND);
            }
            Pix p, src;
            ASSERT(decode_buf(out, ol, p));
            ASSERT(decode_buf(b.data, b.len, src));
            ASSERT(diff(p.img, src.img) == 0.0); /* PNG, VP8L, TIFF: lossless */
            ic_free(out);
        }
    }
    /* Lossy WebP stays lossy (no VP8L), upright. */
    Buf b;
    ASSERT(load("webplossy_o6.webp", b));
    uint8_t* out = nullptr; size_t ol = 0;
    ASSERT(ic_strip_exif(b.data, b.len, &out, &ol) == IC_OK);
    bool vp8l = false;
    for (size_t i = 12; i + 4 <= ol; i++) if (memcmp(out + i, "VP8L", 4) == 0) vp8l = true;
    ASSERT(!vp8l);
    Pix p, r;
    ASSERT(decode_buf(out, ol, p));
    ASSERT(decode_file(ref(6), r));
    ASSERT(diff(p.img, r.img) <= 5.0);
    ic_free(out);
}

/* ── Lossless JPEG rotate / crop ─────────────────────────────────────── */

SAMPLE_TEST(orientation_jpeg_lossless_rotate_all) {
    const ICRotation rots[] = { IC_ROTATE_90, IC_ROTATE_180, IC_ROTATE_270 };
    double worst = 0;
    for (int n = 1; n <= 8; n++) {
        Buf b;
        if (!load(fixture(0, n), b)) { printf("[SKIP] "); return; }
        Pix src;  /* the displayed image */
        ASSERT(decode_buf(b.data, b.len, src));
        for (ICRotation rot : rots) {
            uint8_t* out = nullptr; size_t ol = 0;
            ASSERT(ic_jpeg_lossless_rotate(b.data, b.len, rot, &out, &ol) == IC_OK);
            ASSERT(exif_orientation(out, ol) == 1);
            ICImage expect;
            ASSERT(ic_rotate(&src.img, rot, &expect) == IC_OK);
            Pix p;
            ASSERT(decode_buf(out, ol, p));
            ICImageInfo info;
            ASSERT(ic_get_image_info(out, ol, &info) == IC_OK);
            ASSERT(info.width == p.img.width && info.height == p.img.height);
            /* Edge trimming drops < 1 MCU (16 px) from at most two edges. */
            ASSERT(p.img.width <= expect.width && expect.width - p.img.width < 16);
            ASSERT(p.img.height <= expect.height && expect.height - p.img.height < 16);
            double d = 1e9;
            uint32_t ox[2] = { 0, expect.width - p.img.width };
            uint32_t oy[2] = { 0, expect.height - p.img.height };
            for (uint32_t i = 0; i < 2; i++)
                for (uint32_t j = 0; j < 2; j++) {
                    double dd = mean_diff(expect, ox[i], oy[j], p.img);
                    if (dd < d) d = dd;
                }
            if (d > worst) worst = d;
            if (d > 2.0) printf("\n    o%d rot%d: diff %.3f ", n, (int)rot, d);
            ASSERT(d <= 2.0);
            ic_image_free(&expect);
            ic_free(out);
        }
    }
    printf("[max diff %.2f] ", worst);
}

SAMPLE_TEST(orientation_jpeg_lossless_rotate_patches_exif) {
    Buf b;
    if (!load("jpeg_o6.jpg", b)) { printf("[SKIP] "); return; }
    /* o=6 + rotate 270 = identity: Orientation becomes 1, size unchanged. */
    uint8_t* out = nullptr; size_t ol = 0;
    ASSERT(ic_jpeg_lossless_rotate(b.data, b.len, IC_ROTATE_270, &out, &ol) == IC_OK);
    ICImageInfo info;
    ASSERT(ic_get_image_info(out, ol, &info) == IC_OK);
    ASSERT(info.width == 120 && info.height == 80);
    ASSERT(exif_orientation(out, ol) == 1);
    ic_free(out);
}

SAMPLE_TEST(orientation_jpeg_lossless_crop_all) {
    /* Regions in displayed coordinates (portrait for 5–8). */
    const ICCropRegion land = { 24, 16, 40, 32 }, port = { 16, 24, 32, 40 };
    double worst = 0;
    for (int n = 1; n <= 8; n++) {
        Buf b;
        if (!load(fixture(0, n), b)) { printf("[SKIP] "); return; }
        const ICCropRegion& rg = n >= 5 ? port : land;
        uint8_t* out = nullptr; size_t ol = 0;
        ASSERT(ic_jpeg_lossless_crop(b.data, b.len, &rg, &out, &ol) == IC_OK);
        ASSERT(exif_orientation(out, ol) == n);  /* tag kept */
        Pix src, p;
        ASSERT(decode_buf(b.data, b.len, src));
        ASSERT(decode_buf(out, ol, p));
        ICImageInfo info;
        ASSERT(ic_get_image_info(out, ol, &info) == IC_OK);
        ASSERT(info.width == p.img.width && info.height == p.img.height);
        /* MCU alignment may grow the region by < 16 px per axis. */
        ASSERT(p.img.width >= rg.width && p.img.width < rg.width + 16);
        ASSERT(p.img.height >= rg.height && p.img.height < rg.height + 16);
        ICImage want;
        ASSERT(ic_crop(&src.img, &rg, &want) == IC_OK);
        uint32_t bx = 0, by = 0;
        double d = best_sub_diff(p.img, want, 15, &bx, &by);
        /* The output also has to sit where the region is in the source. */
        ICCropRegion where = { rg.x - bx, rg.y - by, p.img.width, p.img.height };
        ICImage around;
        ASSERT(ic_crop(&src.img, &where, &around) == IC_OK);
        double d2 = diff(p.img, around);
        if (d2 > d) d = d2;
        if (d > worst) worst = d;
        if (d > 2.0) printf("\n    o%d: diff %.3f ", n, d);
        ASSERT(d <= 2.0);
        ic_image_free(&want);
        ic_image_free(&around);
        ic_free(out);

        if (n >= 2) {  /* out-of-bounds region (as displayed) */
            ICCropRegion bad = { 0, 0, (n >= 5 ? 81u : 121u), 10 };
            ASSERT(ic_jpeg_lossless_crop(b.data, b.len, &bad, &out, &ol) == IC_ERROR_INVALID_CROP);
        }
    }
    printf("[max diff %.2f] ", worst);
}

/* ── ic_exif_for_output ──────────────────────────────────────────────── */

TEST(exif_for_output_patches_in_place) {
    /* Little-endian: IFD0 {Orientation SHORT 6, ExifIFD -> 38},
     * Exif IFD at 38 {PixelXDimension LONG 640, PixelYDimension SHORT 426}. */
    uint8_t e[] = {
        'I', 'I', 42, 0, 8, 0, 0, 0,
        2, 0,
        0x12, 0x01, 3, 0, 1, 0, 0, 0, 6, 0, 0, 0,
        0x69, 0x87, 4, 0, 1, 0, 0, 0, 38, 0, 0, 0,
        0, 0, 0, 0,
        2, 0,
        0x02, 0xA0, 4, 0, 1, 0, 0, 0, 0x80, 0x02, 0, 0,
        0x03, 0xA0, 3, 0, 1, 0, 0, 0, 0xAA, 0x01, 0, 0,
        0, 0, 0, 0,
    };
    uint8_t* out = nullptr; size_t ol = 0;
    ASSERT(ic_exif_for_output(e, sizeof(e), 1234, 567, &out, &ol) == IC_OK);
    ASSERT(ol == sizeof(e));
    ASSERT(out[18] == 1 && out[19] == 0);                       /* Orientation = 1 */
    ASSERT(out[48] == 0xD2 && out[49] == 0x04 &&                /* 1234 LONG */
           out[50] == 0 && out[51] == 0);
    ASSERT(out[60] == 0x37 && out[61] == 0x02);                 /* 567 SHORT */
    /* everything else untouched */
    uint8_t want[sizeof(e)];
    memcpy(want, e, sizeof(e));
    want[18] = 1; want[48] = 0xD2; want[49] = 0x04; want[60] = 0x37; want[61] = 0x02;
    ASSERT(memcmp(out, want, sizeof(e)) == 0);
    ASSERT(e[18] == 6); /* input not modified */
    ic_free(out);

    /* "Exif\0\0" prefix is dropped; big-endian works. */
    uint8_t be[6 + 26] = { 'E', 'x', 'i', 'f', 0, 0,
        'M', 'M', 0, 42, 0, 0, 0, 8, 0, 1, 0x01, 0x12, 0, 3, 0, 0, 0, 1, 0, 8, 0, 0, 0, 0, 0, 0 };
    ASSERT(ic_exif_for_output(be, sizeof(be), 1, 1, &out, &ol) == IC_OK);
    ASSERT(ol == 26 && out[0] == 'M' && out[18] == 0 && out[19] == 1);
    ic_free(out);

    uint8_t junk[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    ASSERT(ic_exif_for_output(junk, sizeof(junk), 1, 1, &out, &ol) == IC_ERROR_INVALID_INPUT);
    ASSERT(ic_exif_for_output(nullptr, 0, 1, 1, &out, &ol) == IC_ERROR_INVALID_INPUT);
}

TEST(exif_for_output_roundtrip_with_encode) {
    /* Pixels decoded upright + EXIF patched to Orientation 1 -> displays as-is. */
    uint8_t e[6 + 26] = { 'E', 'x', 'i', 'f', 0, 0,
        'M', 'M', 0, 42, 0, 0, 0, 8, 0, 1, 0x01, 0x12, 0, 3, 0, 0, 0, 1, 0, 6, 0, 0, 0, 0, 0, 0 };
    ICImage img = make_test_image(48, 32);
    uint8_t* meta = nullptr; size_t ml = 0;
    ASSERT(ic_exif_for_output(e, sizeof(e), 48, 32, &meta, &ml) == IC_OK);
    const ICFormat fmts[] = { IC_FORMAT_JPEG, IC_FORMAT_PNG, IC_FORMAT_WEBP };
    for (ICFormat f : fmts) {
        uint8_t* out = nullptr; size_t ol = 0;
        ASSERT(ic_encode_with_exif(&img, f, nullptr, meta, ml, &out, &ol) == IC_OK);
        ICImageInfo info;
        ASSERT(ic_get_image_info(out, ol, &info) == IC_OK);
        ASSERT(info.width == 48 && info.height == 32);
        ASSERT(exif_orientation(out, ol) == 1);
        ic_free(out);
    }
    ic_free(meta);
    ic_image_free(&img);
}
