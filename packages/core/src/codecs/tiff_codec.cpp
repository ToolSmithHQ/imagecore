#include "../imagecore.h"
#include "../ops/orientation.h"

#include <cstdlib>
#include <cstring>
#include <tiffio.h>

#include <algorithm>
#include <vector>

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
    /* libtiff seeks past the end to word-align IFDs; zero the gap instead of
     * leaking uninitialised heap bytes into the file. */
    if (m->offset > m->len) memset(m->data + m->len, 0, m->offset - m->len);
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

extern "C" int tiff_get_exif(const uint8_t* data, size_t len,
                             uint8_t** out_data, size_t* out_len);

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int tiff_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    ic_tiff_membuf buf = { const_cast<uint8_t*>(data), len, len, 0 };
    TIFF* tif = tiff_open_read(&buf);
    if (!tif) return IC_ERROR_DECODE_FAILED;

    uint32_t w, h;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);

    /* Report displayed dimensions: orientations 5–8 swap width and height. */
    uint16_t orientation = ORIENTATION_TOPLEFT;
    TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);
    bool swap = orientation >= ORIENTATION_LEFTTOP && orientation <= ORIENTATION_LEFTBOT;
    out->width = swap ? h : w;
    out->height = swap ? w : h;
    out->format = IC_FORMAT_TIFF;

    TIFFClose(tif);

    uint8_t* exif = nullptr;
    size_t exif_len = 0;
    out->has_exif = tiff_get_exif(data, len, &exif, &exif_len) == IC_OK ? 1 : 0;
    free(exif);
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
     *
     * Orientation: libtiff only handles the flips (1–4) correctly; for 5–8 it
     * silently drops the transpose. So ask for the raster in the file's own
     * orientation (no flipping: rows/columns exactly as stored) and apply the
     * full Orientation transform ourselves.
     */
    uint16_t orientation = ORIENTATION_TOPLEFT;
    TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);
    if (orientation < ORIENTATION_TOPLEFT || orientation > ORIENTATION_LEFTBOT)
        orientation = ORIENTATION_TOPLEFT;
    if (!TIFFReadRGBAImageOriented(tif, w, h,
                                    reinterpret_cast<uint32_t*>(out->pixels),
                                    orientation, 0)) {
        free(out->pixels);
        out->pixels = nullptr;
        TIFFClose(tif);
        return IC_ERROR_DECODE_FAILED;
    }

    int err = ic_orient_image(out, orientation);
    if (err != IC_OK) {
        free(out->pixels);
        out->pixels = nullptr;
        TIFFClose(tif);
        return err;
    }

    TIFFClose(tif);
    return IC_OK;
}

/* ── EXIF embedding ──────────────────────────────────────────────────── */
/*
 * A TIFF file can't hold an opaque EXIF blob: EXIF *is* TIFF IFDs. So the
 * caller's EXIF block (its own little TIFF structure) is merged into the file
 * libtiff wrote:
 *   - descriptive IFD0 tags (Make, Model, Artist, DateTime, ...) are added to
 *     the image IFD, minus tags tied to the pixel data or strip layout and any
 *     tag libtiff already wrote;
 *   - the EXIF, GPS and Interoperability IFDs are copied as sub-IFDs and
 *     pointed to from IFD0 (ExifIFD 0x8769, GPSInfo 0x8825, Interop 0xA005).
 * Values are copied exactly (byte-swapped if the block's byte order differs
 * from the file's). The new IFDs and a rebuilt IFD0 are appended to the file
 * and the header is re-pointed; libtiff's original IFD0 becomes dead bytes.
 * MakerNote (0x927C) is dropped: most maker notes hold offsets relative to the
 * original TIFF header and would be corrupt after relocation. IFD1 (the
 * thumbnail) is not copied.
 */

namespace {

struct ICTiffEntry {
    uint16_t tag = 0;
    uint16_t type = 0;
    uint32_t count = 0;
    std::vector<uint8_t> data;  /* value bytes, already in output byte order */
    uint8_t raw[4] = {0, 0, 0, 0};
    bool keep_raw = false;      /* entry from libtiff's IFD0: copy 12 bytes verbatim */
    int child = -1;             /* sub-IFD index for pointer tags */
};

using ICTiffIfd = std::vector<ICTiffEntry>;

static int ic_tiff_type_size(uint16_t t) {
    switch (t) {
        case 1: case 2: case 6: case 7: return 1;   /* BYTE ASCII SBYTE UNDEFINED */
        case 3: case 8: return 2;                   /* SHORT SSHORT */
        case 4: case 9: case 11: case 13: return 4; /* LONG SLONG FLOAT IFD */
        case 5: case 10: case 12: return 8;         /* RATIONAL SRATIONAL DOUBLE */
        default: return 0;
    }
}

/* Width of the units that need byte-swapping (rationals are two LONGs). */
static int ic_tiff_swap_unit(uint16_t t) {
    switch (t) {
        case 3: case 8: return 2;
        case 4: case 9: case 11: case 13: case 5: case 10: return 4;
        case 12: return 8;
        default: return 1;
    }
}

static bool ic_is_ifd_pointer(uint16_t tag) {
    return tag == 0x8769 || tag == 0x8825 || tag == 0xA005;
}

/* IFD0 tags that describe the source file's pixel layout, not the photo. */
static bool ic_tiff_structural_tag(uint16_t tag) {
    switch (tag) {
        case 254: case 255: case 256: case 257: case 258: case 259:
        case 262: case 263: case 264: case 265: case 266: case 273:
        case 274: case 277: case 278: case 279: case 280: case 281:
        case 284: case 288: case 289: case 290: case 291: case 292:
        case 293: case 317: case 320: case 322: case 323: case 324:
        case 325: case 330: case 338: case 339: case 340: case 341:
        case 347: case 529: case 530: case 531: case 532:
            return true;
        default:
            return tag >= 512 && tag <= 521; /* old-style JPEG, incl. thumbnail offsets */
    }
}

struct ICTiffReader {
    const uint8_t* p;
    size_t n;
    bool be;
    bool out_be;
    std::vector<ICTiffIfd> ifds;
    std::vector<uint32_t> visited;

    uint16_t u16(size_t o) const {
        return be ? (uint16_t)((p[o] << 8) | p[o + 1]) : (uint16_t)((p[o + 1] << 8) | p[o]);
    }
    uint32_t u32(size_t o) const {
        return be ? ((uint32_t)p[o] << 24) | ((uint32_t)p[o + 1] << 16) | ((uint32_t)p[o + 2] << 8) | p[o + 3]
                  : ((uint32_t)p[o + 3] << 24) | ((uint32_t)p[o + 2] << 16) | ((uint32_t)p[o + 1] << 8) | p[o];
    }

    /* Returns the index of the parsed IFD in `ifds`, or -1. */
    int parse(uint32_t off, int depth) {
        if (depth > 3 || (size_t)off + 2 > n) return -1;
        if (std::find(visited.begin(), visited.end(), off) != visited.end()) return -1;
        visited.push_back(off);

        int idx = (int)ifds.size();
        ifds.emplace_back();
        ICTiffIfd entries;

        size_t count = u16(off);
        size_t avail = (n - off - 2) / 12;
        if (count > avail) count = avail;
        for (size_t i = 0; i < count; i++) {
            size_t e = off + 2 + i * 12;
            ICTiffEntry ent;
            ent.tag = u16(e);
            ent.type = u16(e + 2);
            ent.count = u32(e + 4);

            if (ic_is_ifd_pointer(ent.tag)) {
                if ((ent.type == 4 || ent.type == 13) && ent.count == 1) {
                    ent.child = parse(u32(e + 8), depth + 1);
                    if (ent.child >= 0) {
                        ent.type = 4;
                        entries.push_back(std::move(ent));
                    }
                }
                continue;
            }
            if (ent.tag == 0x927C) continue; /* MakerNote: see above */

            int ts = ic_tiff_type_size(ent.type);
            if (!ts) continue;
            uint64_t size = (uint64_t)ts * ent.count;
            if (size == 0 || size > n) continue;
            size_t src;
            if (size <= 4) {
                src = e + 8;
            } else {
                src = u32(e + 8);
                if (src + size > n) continue;
            }
            ent.data.assign(p + src, p + src + size);
            int unit = ic_tiff_swap_unit(ent.type);
            if (be != out_be && unit > 1) {
                for (size_t k = 0; k + unit <= ent.data.size(); k += unit)
                    std::reverse(ent.data.begin() + k, ent.data.begin() + k + unit);
            }
            entries.push_back(std::move(ent));
        }
        ifds[idx] = std::move(entries);
        return idx;
    }
};

struct ICTiffWriter {
    std::vector<uint8_t> tail;  /* bytes appended after the libtiff output */
    uint64_t base;              /* file offset of tail[0] */
    bool be;
    const std::vector<ICTiffIfd>* ifds;

    uint64_t pos() const { return base + tail.size(); }
    void align() { if (pos() & 1) tail.push_back(0); }
    void put16(uint16_t v) {
        if (be) { tail.push_back(v >> 8); tail.push_back(v & 0xFF); }
        else    { tail.push_back(v & 0xFF); tail.push_back(v >> 8); }
    }
    void put32(uint32_t v) {
        if (be) { put16(v >> 16); put16(v & 0xFFFF); }
        else    { put16(v & 0xFFFF); put16(v >> 16); }
    }

    /* Writes `entries` (sub-IFDs first) and returns the IFD's file offset. */
    uint64_t write(ICTiffIfd entries, uint32_t next_ifd) {
        for (auto& e : entries) {
            if (e.child >= 0) {
                uint64_t off = write((*ifds)[e.child], 0);
                e.data.resize(4);
                uint8_t* d = e.data.data();
                uint32_t v = (uint32_t)off;
                if (be) { d[0] = v >> 24; d[1] = v >> 16; d[2] = v >> 8; d[3] = v; }
                else    { d[0] = v; d[1] = v >> 8; d[2] = v >> 16; d[3] = v >> 24; }
                e.child = -1;
            }
        }
        std::stable_sort(entries.begin(), entries.end(),
                         [](const ICTiffEntry& a, const ICTiffEntry& b) { return a.tag < b.tag; });
        entries.erase(std::unique(entries.begin(), entries.end(),
                                  [](const ICTiffEntry& a, const ICTiffEntry& b) { return a.tag == b.tag; }),
                      entries.end());

        align();
        uint64_t ifd_off = pos();
        uint64_t value_off = ifd_off + 2 + 12 * entries.size() + 4;
        std::vector<uint64_t> offs(entries.size(), 0);
        for (size_t i = 0; i < entries.size(); i++) {
            if (!entries[i].keep_raw && entries[i].data.size() > 4) {
                value_off += value_off & 1;
                offs[i] = value_off;
                value_off += entries[i].data.size();
            }
        }

        put16((uint16_t)entries.size());
        for (size_t i = 0; i < entries.size(); i++) {
            const ICTiffEntry& e = entries[i];
            put16(e.tag);
            put16(e.type);
            put32(e.count);
            if (e.keep_raw) {
                tail.insert(tail.end(), e.raw, e.raw + 4);
            } else if (e.data.size() <= 4) {
                tail.insert(tail.end(), e.data.begin(), e.data.end());
                for (size_t k = e.data.size(); k < 4; k++) tail.push_back(0);
            } else {
                put32((uint32_t)offs[i]);
            }
        }
        put32(next_ifd);
        for (size_t i = 0; i < entries.size(); i++) {
            if (offs[i]) {
                align();
                tail.insert(tail.end(), entries[i].data.begin(), entries[i].data.end());
            }
        }
        return ifd_off;
    }
};

} /* namespace */

/*
 * Merge `exif` into the classic TIFF in buf (written by libtiff). On success
 * buf grows in place. Returns false (buf untouched) if either structure is
 * unusable, in which case the caller keeps the file without EXIF.
 */
static bool tiff_merge_exif(ic_tiff_membuf* buf, const uint8_t* exif, size_t exif_len) {
    const uint8_t* f = buf->data;
    size_t flen = buf->len;
    if (flen < 8 || exif_len < 8) return false;
    bool out_be = f[0] == 'M';

    /* libtiff's IFD0 */
    ICTiffReader file = { f, flen, out_be, out_be, {}, {} };
    if (file.u16(2) != 42) return false;
    uint32_t ifd0 = file.u32(4);
    if ((size_t)ifd0 + 2 > flen) return false;
    size_t n0 = file.u16(ifd0);
    if ((size_t)ifd0 + 2 + n0 * 12 + 4 > flen) return false;
    uint32_t next_ifd = file.u32(ifd0 + 2 + n0 * 12);

    ICTiffIfd merged;
    for (size_t i = 0; i < n0; i++) {
        size_t e = ifd0 + 2 + i * 12;
        ICTiffEntry ent;
        ent.tag = file.u16(e);
        ent.type = file.u16(e + 2);
        ent.count = file.u32(e + 4);
        memcpy(ent.raw, f + e + 8, 4);
        ent.keep_raw = true;
        merged.push_back(std::move(ent));
    }
    const size_t own = merged.size();

    /* The caller's EXIF block */
    ICTiffReader src = { exif, exif_len, exif[0] == 'M', out_be, {}, {} };
    if (src.u16(2) != 42) return false;
    int root = src.parse(src.u32(4), 0);
    if (root < 0) return false;

    for (const ICTiffEntry& e : src.ifds[root]) {
        if (e.child >= 0 && e.tag == 0xA005) continue; /* Interop belongs in ExifIFD */
        if (e.child < 0 && ic_tiff_structural_tag(e.tag)) continue;
        bool dup = false;
        for (size_t i = 0; i < own && !dup; i++) dup = merged[i].tag == e.tag;
        if (!dup) merged.push_back(e);
    }
    if (merged.size() == own) return true; /* nothing worth adding */

    ICTiffWriter w;
    w.base = flen + (flen & 1);
    w.be = out_be;
    w.ifds = &src.ifds;
    uint64_t new_ifd0 = w.write(std::move(merged), next_ifd);
    uint64_t total = w.base + w.tail.size();
    if (total > 0xFFFFFFFFull) return false;

    if (total > buf->capacity) {
        uint8_t* tmp = static_cast<uint8_t*>(realloc(buf->data, total));
        if (!tmp) return false;
        buf->data = tmp;
        buf->capacity = total;
    }
    if (flen & 1) buf->data[flen] = 0;
    memcpy(buf->data + w.base, w.tail.data(), w.tail.size());
    buf->len = total;

    uint8_t* h = buf->data + 4;
    uint32_t v = (uint32_t)new_ifd0;
    if (out_be) { h[0] = v >> 24; h[1] = v >> 16; h[2] = v >> 8; h[3] = v; }
    else        { h[0] = v; h[1] = v >> 8; h[2] = v >> 16; h[3] = v >> 24; }
    return true;
}

/* ── EXIF extraction ─────────────────────────────────────────────────── */

/*
 * A TIFF file's "EXIF" is its own IFD0 plus the Exif/GPS sub-IFDs. Rebuild
 * those as a compact standalone TIFF structure (same byte order, offsets
 * relative to the new header): IFD0 minus pixel-layout tags (strip/tile
 * offsets and byte counts, SubIFDs, compression, ...), plus ExifIFD, GPS and
 * Interop sub-IFDs. No pixel data, no thumbnail IFD1, no MakerNote, no ICC
 * profile (tag 34675).
 * Returns IC_ERROR_EXIF_NOT_FOUND when nothing but layout/resolution tags
 * remains (e.g. after ic_strip_exif).
 */
extern "C" int tiff_get_exif(const uint8_t* data, size_t len,
                             uint8_t** out_data, size_t* out_len) {
    if (len < 8) return IC_ERROR_EXIF_NOT_FOUND;
    bool be = data[0] == 'M';
    try {
        ICTiffReader src = { data, len, be, be, {}, {} };
        if (src.u16(2) != 42) return IC_ERROR_EXIF_NOT_FOUND; /* BigTIFF etc. */
        int root = src.parse(src.u32(4), 0);
        if (root < 0) return IC_ERROR_EXIF_NOT_FOUND;

        ICTiffIfd ifd0;
        bool meaningful = false;
        for (const ICTiffEntry& e : src.ifds[root]) {
            if (e.child >= 0 && e.tag == 0xA005) continue; /* Interop belongs in ExifIFD */
            if (e.child < 0 && ic_tiff_structural_tag(e.tag)) continue;
            /* ICC profile: colour data, not EXIF; other formats carry it in
             * their own containers (iCCP / APP2 / colr). */
            if (e.tag == 34675) continue;
            if (e.tag != 282 && e.tag != 283 && e.tag != 296) meaningful = true;
            ifd0.push_back(e);
        }
        if (!meaningful) return IC_ERROR_EXIF_NOT_FOUND;

        ICTiffWriter w;
        w.base = 8;
        w.be = be;
        w.ifds = &src.ifds;
        uint64_t ifd0_off = w.write(std::move(ifd0), 0);
        size_t total = 8 + w.tail.size();

        uint8_t* out = static_cast<uint8_t*>(malloc(total));
        if (!out) return IC_ERROR_ALLOC_FAILED;
        uint32_t v = (uint32_t)ifd0_off;
        if (be) {
            const uint8_t h[8] = { 'M', 'M', 0, 42, (uint8_t)(v >> 24), (uint8_t)(v >> 16),
                                   (uint8_t)(v >> 8), (uint8_t)v };
            memcpy(out, h, 8);
        } else {
            const uint8_t h[8] = { 'I', 'I', 42, 0, (uint8_t)v, (uint8_t)(v >> 8),
                                   (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
            memcpy(out, h, 8);
        }
        memcpy(out + 8, w.tail.data(), w.tail.size());
        *out_data = out;
        *out_len = total;
        return IC_OK;
    } catch (...) {
        return IC_ERROR_ALLOC_FAILED;
    }
}

/* ── Encode ──────────────────────────────────────────────────────────── */

extern "C" int tiff_encode(const ICImage* img, const ICEncodeOpts* opts,
                           const uint8_t* exif, size_t exif_len,
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

    if (exif && exif_len > 0) {
        try {
            tiff_merge_exif(&buf, exif, exif_len);
        } catch (...) { /* std::bad_alloc: keep the file without EXIF */
        }
    }

    *out_data = buf.data;
    *out_len = buf.len;
    return IC_OK;
}
