/*
 * Image orientation helpers.
 *
 * Orientation values follow EXIF / TIFF tag 0x0112. Each value names the
 * transform that turns the *stored* pixels into the *displayed* image:
 *   1 none         2 mirror horizontally   3 rotate 180   4 mirror vertically
 *   5 transpose    6 rotate 90 CW          7 transverse   8 rotate 270 CW
 * (5–8 swap width and height.)
 *
 * The same eight values are used internally for every orientation source
 * (EXIF in JPEG/PNG/WebP, the TIFF Orientation tag, AVIF irot/imir) so they
 * can be composed and mapped onto libjpeg-turbo's lossless transforms.
 */

#include "../imagecore.h"
#include "orientation.h"

#include <cstdlib>
#include <cstring>

/* ── Orientation algebra ─────────────────────────────────────────────── */

/*
 * Each orientation as a 2x2 integer matrix acting on pixel coordinates
 * centred on the image (x right, y down): displayed = M * stored.
 */
static const int kOrientMatrix[9][4] = {
    { 1,  0,  0,  1},  /* (unused) */
    { 1,  0,  0,  1},  /* 1 identity */
    {-1,  0,  0,  1},  /* 2 mirror H */
    {-1,  0,  0, -1},  /* 3 rotate 180 */
    { 1,  0,  0, -1},  /* 4 mirror V */
    { 0,  1,  1,  0},  /* 5 transpose */
    { 0, -1,  1,  0},  /* 6 rotate 90 CW */
    { 0, -1, -1,  0},  /* 7 transverse */
    { 0,  1, -1,  0},  /* 8 rotate 270 CW */
};

/* Orientation equivalent to applying `first`, then `then`. */
extern "C" int ic_orientation_compose(int first, int then) {
    if (first < 1 || first > 8) first = 1;
    if (then < 1 || then > 8) then = 1;
    const int* a = kOrientMatrix[then];
    const int* b = kOrientMatrix[first];
    int m[4] = {
        a[0] * b[0] + a[1] * b[2], a[0] * b[1] + a[1] * b[3],
        a[2] * b[0] + a[3] * b[2], a[2] * b[1] + a[3] * b[3],
    };
    for (int o = 1; o <= 8; o++)
        if (memcmp(m, kOrientMatrix[o], sizeof(m)) == 0) return o;
    return 1; /* unreachable: the 8 matrices form a group */
}

/* ── EXIF parsing helpers ────────────────────────────────────────────── */

static inline uint16_t rd16(const uint8_t* p, bool le) {
    return le ? (uint16_t)(p[0] | (p[1] << 8)) : (uint16_t)((p[0] << 8) | p[1]);
}
static inline uint32_t rd32(const uint8_t* p, bool le) {
    return le ? ((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24))
              : (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}
static inline void wr16(uint8_t* p, uint16_t v, bool le) {
    if (le) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
    else    { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
}
static inline void wr32(uint8_t* p, uint32_t v, bool le) {
    if (le) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
    else    { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
}

/* Skip an optional "Exif\0\0" prefix; check for a TIFF header. */
static bool tiff_block(const uint8_t*& p, size_t& n, bool& le) {
    if (n >= 6 && memcmp(p, "Exif\0\0", 6) == 0) { p += 6; n -= 6; }
    if (n < 8) return false;
    if (p[0] == 'I' && p[1] == 'I') le = true;
    else if (p[0] == 'M' && p[1] == 'M') le = false;
    else return false;
    return rd16(p + 2, le) == 42;
}

/*
 * EXIF Orientation (1–8) from a raw EXIF block (TIFF structure, optional
 * "Exif\0\0" prefix). Reads IFD0 only. Returns 1 when absent/invalid.
 */
extern "C" int ic_exif_orientation(const uint8_t* exif, size_t len) {
    if (!exif) return 1;
    const uint8_t* p = exif;
    size_t n = len;
    bool le;
    if (!tiff_block(p, n, le)) return 1;
    uint32_t ifd = rd32(p + 4, le);
    if (ifd > n || n - ifd < 2) return 1;
    uint16_t count = rd16(p + ifd, le);
    for (uint32_t i = 0; i < count; i++) {
        size_t e = (size_t)ifd + 2 + (size_t)i * 12;
        if (e + 12 > n) break;
        if (rd16(p + e, le) == 0x0112) {
            uint16_t type = rd16(p + e + 2, le);
            uint32_t v = type == 4 ? rd32(p + e + 8, le) : rd16(p + e + 8, le);
            return (v >= 1 && v <= 8) ? (int)v : 1;
        }
    }
    return 1;
}

/*
 * Patch an EXIF block in place: Orientation -> 1 and, when set_dims,
 * PixelXDimension/PixelYDimension -> width/height. Only inline SHORT/LONG
 * values are touched, so the structure is unchanged. `p` must point at the
 * TIFF header (see ic_exif_patch_for_output for the public-ish wrapper).
 */
static void patch_ifd(uint8_t* p, size_t n, bool le, uint32_t ifd, int depth,
                      uint32_t* seen, int* nseen,
                      bool set_dims, uint32_t width, uint32_t height) {
    if (depth > 2 || ifd > n || n - ifd < 2) return;
    for (int i = 0; i < *nseen; i++) if (seen[i] == ifd) return;
    if (*nseen < 8) seen[(*nseen)++] = ifd;
    uint16_t count = rd16(p + ifd, le);
    for (uint32_t i = 0; i < count; i++) {
        size_t e = (size_t)ifd + 2 + (size_t)i * 12;
        if (e + 12 > n) return;
        uint16_t tag = rd16(p + e, le), type = rd16(p + e + 2, le);
        auto set_inline = [&](uint32_t v) {
            if (type == 3) wr16(p + e + 8, (uint16_t)v, le);      /* SHORT */
            else if (type == 4) wr32(p + e + 8, v, le);           /* LONG */
        };
        if (tag == 0x0112) set_inline(1);
        else if (tag == 0xA002 && set_dims) set_inline(width);
        else if (tag == 0xA003 && set_dims) set_inline(height);
        else if (tag == 0x8769)
            patch_ifd(p, n, le, rd32(p + e + 8, le), depth + 1, seen, nseen,
                      set_dims, width, height);
    }
}

extern "C" int ic_exif_patch_for_output(uint8_t* exif, size_t len, int set_dims,
                                        uint32_t width, uint32_t height) {
    const uint8_t* cp = exif;
    size_t n = len;
    bool le;
    if (!exif || !tiff_block(cp, n, le)) return IC_ERROR_INVALID_INPUT;
    uint8_t* p = exif + (cp - exif);
    uint32_t seen[8];
    int nseen = 0;
    patch_ifd(p, n, le, rd32(p + 4, le), 0, seen, &nseen, set_dims != 0, width, height);
    return IC_OK;
}

extern "C" int ic_exif_for_output(const uint8_t* exif, size_t len,
                                  uint32_t width, uint32_t height,
                                  uint8_t** out, size_t* out_len) {
    if (!exif || !len || !out || !out_len) return IC_ERROR_INVALID_INPUT;
    *out = nullptr;
    *out_len = 0;
    const uint8_t* p = exif;
    size_t n = len;
    bool le;
    if (!tiff_block(p, n, le)) return IC_ERROR_INVALID_INPUT;
    uint8_t* buf = static_cast<uint8_t*>(malloc(n));
    if (!buf) return IC_ERROR_ALLOC_FAILED;
    memcpy(buf, p, n);
    ic_exif_patch_for_output(buf, n, 1, width, height);
    *out = buf;
    *out_len = n;
    return IC_OK;
}

/*
 * Minimal EXIF block (big-endian TIFF) holding only IFD0 Orientation.
 * Writes 26 bytes to `out` (no "Exif\0\0" prefix).
 */
extern "C" size_t ic_exif_orientation_only(int orientation, uint8_t out[26]) {
    static const uint8_t tmpl[26] = {
        'M', 'M', 0, 42, 0, 0, 0, 8,   /* header, IFD0 at 8 */
        0, 1,                          /* 1 entry */
        0x01, 0x12, 0, 3, 0, 0, 0, 1, 0, 1, 0, 0,  /* Orientation SHORT x1 */
        0, 0, 0, 0,                    /* no next IFD */
    };
    memcpy(out, tmpl, sizeof(tmpl));
    out[19] = (uint8_t)orientation;
    return sizeof(tmpl);
}

/* ── Pixel transform ─────────────────────────────────────────────────── */

/*
 * Replace img's pixels with the displayed image for orientation o.
 * Displayed pixel (x, y) takes the stored pixel at:
 *   2 (W-1-x, y)   3 (W-1-x, H-1-y)   4 (x, H-1-y)
 *   5 (y, x)       6 (y, H-1-x)       7 (W-1-y, H-1-x)   8 (W-1-y, x)
 */
extern "C" int ic_orient_image(ICImage* img, int o) {
    if (!img || !img->pixels) return IC_ERROR_INVALID_INPUT;
    if (o <= 1 || o > 8) return IC_OK;

    const uint32_t W = img->width, H = img->height;
    const bool swap = o >= 5;
    const uint32_t DW = swap ? H : W, DH = swap ? W : H;
    const size_t dstride = (size_t)DW * 4;
    uint8_t* dst = static_cast<uint8_t*>(malloc(dstride * DH));
    if (!dst) return IC_ERROR_ALLOC_FAILED;

    const uint8_t* src = img->pixels;
    const size_t sstride = img->stride;
    for (uint32_t y = 0; y < DH; y++) {
        uint8_t* row = dst + (size_t)y * dstride;
        for (uint32_t x = 0; x < DW; x++) {
            uint32_t sx, sy;
            switch (o) {
                case 2:  sx = W - 1 - x; sy = y;         break;
                case 3:  sx = W - 1 - x; sy = H - 1 - y; break;
                case 4:  sx = x;         sy = H - 1 - y; break;
                case 5:  sx = y;         sy = x;         break;
                case 6:  sx = y;         sy = H - 1 - x; break;
                case 7:  sx = W - 1 - y; sy = H - 1 - x; break;
                default: sx = W - 1 - y; sy = x;         break; /* 8 */
            }
            memcpy(row + (size_t)x * 4, src + (size_t)sy * sstride + (size_t)sx * 4, 4);
        }
    }

    free(img->pixels);
    img->pixels = dst;
    img->width = DW;
    img->height = DH;
    img->stride = (uint32_t)dstride;
    return IC_OK;
}

/*
 * Map a rectangle in displayed coordinates (image displayed as DW x DH) to
 * stored coordinates for orientation o. The region must lie within bounds.
 */
extern "C" void ic_orient_region_to_stored(int o, uint32_t stored_w, uint32_t stored_h,
                                           const ICCropRegion* d, ICCropRegion* s) {
    const uint32_t W = stored_w, H = stored_h;
    const uint32_t x0 = d->x, y0 = d->y, x1 = d->x + d->width - 1, y1 = d->y + d->height - 1;
    uint32_t ax, ay, bx, by; /* stored positions of the two corners */
    switch (o) {
        case 2:  ax = W - 1 - x0; ay = y0;         bx = W - 1 - x1; by = y1;         break;
        case 3:  ax = W - 1 - x0; ay = H - 1 - y0; bx = W - 1 - x1; by = H - 1 - y1; break;
        case 4:  ax = x0;         ay = H - 1 - y0; bx = x1;         by = H - 1 - y1; break;
        case 5:  ax = y0;         ay = x0;         bx = y1;         by = x1;         break;
        case 6:  ax = y0;         ay = H - 1 - x0; bx = y1;         by = H - 1 - x1; break;
        case 7:  ax = W - 1 - y0; ay = H - 1 - x0; bx = W - 1 - y1; by = H - 1 - x1; break;
        case 8:  ax = W - 1 - y0; ay = x0;         bx = W - 1 - y1; by = x1;         break;
        default: *s = *d; return;
    }
    s->x = ax < bx ? ax : bx;
    s->y = ay < by ? ay : by;
    s->width = (ax < bx ? bx - ax : ax - bx) + 1;
    s->height = (ay < by ? by - ay : ay - by) + 1;
}
