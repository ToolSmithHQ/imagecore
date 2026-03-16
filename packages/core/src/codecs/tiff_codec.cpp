#include "../imagecore.h"

#include <cstdlib>
#include <cstring>
#include <tiffio.h>

/* ── In-memory TIFF I/O ─────────────────────────────────────────────── */

struct ic_tiff_membuf {
    uint8_t* data;
    size_t   len;       /* bytes written (for write) or total size (for read) */
    size_t   capacity;  /* allocated size (write only) */
    toff_t   offset;
};

static tmsize_t ic_tiff_mem_read(thandle_t h, void* buf, tmsize_t size) {
    auto* m = static_cast<ic_tiff_membuf*>(h);
    tmsize_t avail = static_cast<tmsize_t>(m->len - m->offset);
    if (avail <= 0) return 0;
    if (size > avail) size = avail;
    memcpy(buf, m->data + m->offset, size);
    m->offset += size;
    return size;
}

static tmsize_t ic_tiff_mem_write(thandle_t h, void* buf, tmsize_t size) {
    auto* m = static_cast<ic_tiff_membuf*>(h);
    size_t needed = m->offset + size;
    if (needed > m->capacity) {
        size_t new_cap = m->capacity * 2;
        if (new_cap < needed) new_cap = needed;
        uint8_t* tmp = static_cast<uint8_t*>(realloc(m->data, new_cap));
        if (!tmp) return 0;
        m->data = tmp;
        m->capacity = new_cap;
    }
    memcpy(m->data + m->offset, buf, size);
    m->offset += size;
    if (m->offset > m->len) m->len = m->offset;
    return size;
}

static tmsize_t ic_tiff_write_noop(thandle_t, void*, tmsize_t) { return 0; }

static toff_t ic_tiff_mem_seek(thandle_t h, toff_t off, int whence) {
    auto* m = static_cast<ic_tiff_membuf*>(h);
    switch (whence) {
        case SEEK_SET: m->offset = off; break;
        case SEEK_CUR: m->offset += off; break;
        case SEEK_END: m->offset = m->len + off; break;
    }
    return static_cast<toff_t>(m->offset);
}

static int ic_tiff_close(thandle_t) { return 0; }

static toff_t ic_tiff_mem_size(thandle_t h) {
    return static_cast<toff_t>(static_cast<ic_tiff_membuf*>(h)->len);
}

/* ── Helper: open TIFF for reading from memory ──────────────────────── */

static TIFF* tiff_open_read(ic_tiff_membuf* buf) {
    TIFFSetWarningHandler(nullptr);
    return TIFFClientOpen("mem", "r", buf,
                           ic_tiff_mem_read, ic_tiff_write_noop,
                           ic_tiff_mem_seek, ic_tiff_close,
                           ic_tiff_mem_size, nullptr, nullptr);
}

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int tiff_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    ic_tiff_membuf buf = { const_cast<uint8_t*>(data), len, len, 0 };
    TIFF* tif = tiff_open_read(&buf);
    if (!tif) return IC_ERROR_DECODE_FAILED;

    uint32_t w, h;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);

    out->width = w;
    out->height = h;
    out->format = IC_FORMAT_TIFF;
    out->has_exif = 0;

    TIFFClose(tif);
    return IC_OK;
}

/* ── Decode ──────────────────────────────────────────────────────────── */

extern "C" int tiff_decode(const uint8_t* data, size_t len, ICImage* out) {
    ic_tiff_membuf buf = { const_cast<uint8_t*>(data), len, len, 0 };
    TIFF* tif = tiff_open_read(&buf);
    if (!tif) return IC_ERROR_DECODE_FAILED;

    uint32_t w, h;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);

    out->width = w;
    out->height = h;
    out->stride = w * 4;

    size_t total = (size_t)w * h * 4;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) {
        TIFFClose(tif);
        return IC_ERROR_ALLOC_FAILED;
    }

    /*
     * TIFFReadRGBAImageOriented outputs ABGR on little-endian machines.
     * On macOS/ARM and x86 (both little-endian), the uint32 pixel layout is:
     *   byte 0 = R, byte 1 = G, byte 2 = B, byte 3 = A
     * which matches our RGBA layout — no swizzle needed.
     */
    if (!TIFFReadRGBAImageOriented(tif, w, h,
                                    reinterpret_cast<uint32_t*>(out->pixels),
                                    ORIENTATION_TOPLEFT, 0)) {
        free(out->pixels);
        out->pixels = nullptr;
        TIFFClose(tif);
        return IC_ERROR_DECODE_FAILED;
    }

    TIFFClose(tif);
    return IC_OK;
}

/* ── Encode ──────────────────────────────────────────────────────────── */

extern "C" int tiff_encode(const ICImage* img, const ICEncodeOpts* opts,
                           uint8_t** out_data, size_t* out_len) {
    (void)opts;
    TIFFSetWarningHandler(nullptr);

    ic_tiff_membuf buf = { nullptr, 0, 65536, 0 };
    buf.data = static_cast<uint8_t*>(malloc(buf.capacity));
    if (!buf.data) return IC_ERROR_ALLOC_FAILED;

    TIFF* tif = TIFFClientOpen("mem", "w", &buf,
                                ic_tiff_mem_read, ic_tiff_mem_write,
                                ic_tiff_mem_seek, ic_tiff_close,
                                ic_tiff_mem_size, nullptr, nullptr);
    if (!tif) {
        free(buf.data);
        return IC_ERROR_ENCODE_FAILED;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, img->width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, img->height);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, img->height);

    uint16_t extra_samples[] = { EXTRASAMPLE_ASSOCALPHA };
    TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, extra_samples);

    for (uint32_t y = 0; y < img->height; y++) {
        if (TIFFWriteScanline(tif, img->pixels + y * img->stride, y, 0) < 0) {
            TIFFClose(tif);
            free(buf.data);
            return IC_ERROR_ENCODE_FAILED;
        }
    }

    TIFFClose(tif);

    *out_data = buf.data;
    *out_len = buf.len;
    return IC_OK;
}
