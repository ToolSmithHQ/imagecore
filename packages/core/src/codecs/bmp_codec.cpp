#include "../imagecore.h"

#include <cstdlib>
#include <cstring>

/*
 * Minimal BMP encoder/decoder — no external dependency needed.
 * Supports: 24-bit RGB and 32-bit RGBA uncompressed BMPs.
 */

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t type;       /* 'BM' */
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
};

struct BMPInfoHeader {
    uint32_t size;        /* 40 for BITMAPINFOHEADER */
    int32_t  width;
    int32_t  height;      /* negative = top-down */
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_ppm;
    int32_t  y_ppm;
    uint32_t colors_used;
    uint32_t colors_important;
};
#pragma pack(pop)

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int bmp_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    if (len < sizeof(BMPFileHeader) + sizeof(BMPInfoHeader))
        return IC_ERROR_INVALID_INPUT;

    const auto* fh = reinterpret_cast<const BMPFileHeader*>(data);
    const auto* ih = reinterpret_cast<const BMPInfoHeader*>(data + sizeof(BMPFileHeader));

    if (fh->type != 0x4D42) return IC_ERROR_INVALID_INPUT; /* 'BM' */

    out->width = static_cast<uint32_t>(ih->width > 0 ? ih->width : -ih->width);
    out->height = static_cast<uint32_t>(ih->height > 0 ? ih->height : -ih->height);
    out->format = IC_FORMAT_BMP;
    out->has_exif = 0;
    return IC_OK;
}

/* ── Decode ──────────────────────────────────────────────────────────── */

extern "C" int bmp_decode(const uint8_t* data, size_t len, ICImage* out) {
    if (len < sizeof(BMPFileHeader) + sizeof(BMPInfoHeader))
        return IC_ERROR_INVALID_INPUT;

    const auto* fh = reinterpret_cast<const BMPFileHeader*>(data);
    const auto* ih = reinterpret_cast<const BMPInfoHeader*>(data + sizeof(BMPFileHeader));

    if (fh->type != 0x4D42) return IC_ERROR_INVALID_INPUT;
    if (ih->compression != 0) return IC_ERROR_UNSUPPORTED_FORMAT; /* BI_RGB only */

    int32_t w = ih->width;
    int32_t h = ih->height;
    int top_down = (h < 0);
    if (h < 0) h = -h;
    if (w <= 0 || h <= 0) return IC_ERROR_INVALID_INPUT;

    uint16_t bpp = ih->bpp;
    if (bpp != 24 && bpp != 32) return IC_ERROR_UNSUPPORTED_FORMAT;

    out->width = static_cast<uint32_t>(w);
    out->height = static_cast<uint32_t>(h);
    out->stride = static_cast<uint32_t>(w) * 4;

    size_t total = (size_t)out->stride * out->height;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) return IC_ERROR_ALLOC_FAILED;

    uint32_t row_size = ((bpp * w + 31) / 32) * 4; /* BMP rows are 4-byte aligned */
    const uint8_t* pixel_data = data + fh->offset;

    for (int32_t y = 0; y < h; y++) {
        int src_y = top_down ? y : (h - 1 - y);
        const uint8_t* src_row = pixel_data + (size_t)src_y * row_size;
        uint8_t* dst_row = out->pixels + y * out->stride;

        for (int32_t x = 0; x < w; x++) {
            if (bpp == 24) {
                dst_row[x * 4 + 0] = src_row[x * 3 + 2]; /* R (BMP is BGR) */
                dst_row[x * 4 + 1] = src_row[x * 3 + 1]; /* G */
                dst_row[x * 4 + 2] = src_row[x * 3 + 0]; /* B */
                dst_row[x * 4 + 3] = 0xFF;                /* A */
            } else { /* 32-bit */
                dst_row[x * 4 + 0] = src_row[x * 4 + 2]; /* R (BMP is BGRA) */
                dst_row[x * 4 + 1] = src_row[x * 4 + 1]; /* G */
                dst_row[x * 4 + 2] = src_row[x * 4 + 0]; /* B */
                dst_row[x * 4 + 3] = src_row[x * 4 + 3]; /* A */
            }
        }
    }

    return IC_OK;
}

/* ── Encode ──────────────────────────────────────────────────────────── */

extern "C" int bmp_encode(const ICImage* img, const ICEncodeOpts* opts,
                          uint8_t** out_data, size_t* out_len) {
    (void)opts;

    uint32_t row_size = ((24 * img->width + 31) / 32) * 4; /* 24-bit, 4-byte aligned */
    uint32_t pixel_size = row_size * img->height;
    uint32_t file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + pixel_size;

    uint8_t* buf = static_cast<uint8_t*>(malloc(file_size));
    if (!buf) return IC_ERROR_ALLOC_FAILED;

    memset(buf, 0, file_size);

    /* File header */
    auto* fh = reinterpret_cast<BMPFileHeader*>(buf);
    fh->type = 0x4D42;
    fh->size = file_size;
    fh->offset = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

    /* Info header */
    auto* ih = reinterpret_cast<BMPInfoHeader*>(buf + sizeof(BMPFileHeader));
    ih->size = sizeof(BMPInfoHeader);
    ih->width = static_cast<int32_t>(img->width);
    ih->height = static_cast<int32_t>(img->height); /* bottom-up */
    ih->planes = 1;
    ih->bpp = 24;
    ih->compression = 0;
    ih->image_size = pixel_size;
    ih->x_ppm = 2835; /* 72 DPI */
    ih->y_ppm = 2835;

    /* Pixel data (bottom-up, BGR) */
    uint8_t* pixel_buf = buf + fh->offset;
    for (uint32_t y = 0; y < img->height; y++) {
        uint32_t src_y = img->height - 1 - y; /* flip vertical */
        const uint8_t* src_row = img->pixels + src_y * img->stride;
        uint8_t* dst_row = pixel_buf + y * row_size;

        for (uint32_t x = 0; x < img->width; x++) {
            dst_row[x * 3 + 0] = src_row[x * 4 + 2]; /* B */
            dst_row[x * 3 + 1] = src_row[x * 4 + 1]; /* G */
            dst_row[x * 3 + 2] = src_row[x * 4 + 0]; /* R */
        }
    }

    *out_data = buf;
    *out_len = file_size;
    return IC_OK;
}
