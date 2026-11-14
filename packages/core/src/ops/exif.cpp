/*
 * EXIF reader — parses APP1 marker from JPEG (and eXIf chunk from PNG).
 * Pure binary parsing, no external dependency.
 *
 * Supports: JPEG (APP1/Exif), basic tag extraction.
 * For other formats, returns IC_ERROR_EXIF_NOT_FOUND.
 */

#include "../imagecore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ── TIFF/EXIF tag IDs ───────────────────────────────────────────────── */

enum ExifTag {
    TAG_IMAGE_WIDTH        = 0x0100,
    TAG_IMAGE_HEIGHT       = 0x0101,
    TAG_MAKE               = 0x010F,
    TAG_MODEL              = 0x0110,
    TAG_ORIENTATION        = 0x0112,
    TAG_X_RESOLUTION       = 0x011A,
    TAG_Y_RESOLUTION       = 0x011B,
    TAG_RESOLUTION_UNIT    = 0x0128,
    TAG_SOFTWARE           = 0x0131,
    TAG_DATE_TIME          = 0x0132,
    TAG_EXIF_IFD_POINTER   = 0x8769,
    TAG_GPS_IFD_POINTER    = 0x8825,
    /* EXIF sub-IFD tags */
    TAG_EXPOSURE_TIME      = 0x829A,
    TAG_FNUMBER            = 0x829D,
    TAG_ISO_SPEED          = 0x8827,
    TAG_DATE_TIME_ORIGINAL = 0x9003,
    TAG_FOCAL_LENGTH       = 0x920A,
    TAG_LENS_MAKE          = 0xA433,
    TAG_LENS_MODEL         = 0xA434,
    /* GPS tags */
    TAG_GPS_LATITUDE_REF   = 0x0001,
    TAG_GPS_LATITUDE       = 0x0002,
    TAG_GPS_LONGITUDE_REF  = 0x0003,
    TAG_GPS_LONGITUDE      = 0x0004,
    TAG_GPS_ALTITUDE       = 0x0006,
};

/* ── TIFF byte order helpers ─────────────────────────────────────────── */

static inline uint16_t read_u16(const uint8_t* p, int big_endian) {
    return big_endian ? (uint16_t)((p[0] << 8) | p[1])
                      : (uint16_t)((p[1] << 8) | p[0]);
}

static inline uint32_t read_u32(const uint8_t* p, int big_endian) {
    return big_endian ? ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                        ((uint32_t)p[2] << 8) | p[3]
                      : ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
                        ((uint32_t)p[1] << 8) | p[0];
}

/* ── EXIF tag value as string ────────────────────────────────────────── */

/*
 * ICExifEntry: internal representation of a parsed EXIF tag.
 * We store up to 64 entries — enough for common fields.
 */
#define IC_MAX_EXIF_ENTRIES 64
#define IC_EXIF_VALUE_LEN 128

struct ICExifEntry {
    uint16_t tag;
    char name[32];
    char value[IC_EXIF_VALUE_LEN];
};

struct ICExifResult {
    ICExifEntry entries[IC_MAX_EXIF_ENTRIES];
    int count;
};

static const char* tag_name(uint16_t tag) {
    switch (tag) {
        case TAG_MAKE:               return "Make";
        case TAG_MODEL:              return "Model";
        case TAG_ORIENTATION:        return "Orientation";
        case TAG_X_RESOLUTION:       return "XResolution";
        case TAG_Y_RESOLUTION:       return "YResolution";
        case TAG_RESOLUTION_UNIT:    return "ResolutionUnit";
        case TAG_SOFTWARE:           return "Software";
        case TAG_DATE_TIME:          return "DateTime";
        case TAG_EXPOSURE_TIME:      return "ExposureTime";
        case TAG_FNUMBER:            return "FNumber";
        case TAG_ISO_SPEED:          return "ISOSpeedRatings";
        case TAG_DATE_TIME_ORIGINAL: return "DateTimeOriginal";
        case TAG_FOCAL_LENGTH:       return "FocalLength";
        case TAG_LENS_MAKE:          return "LensMake";
        case TAG_LENS_MODEL:         return "LensModel";
        case TAG_IMAGE_WIDTH:        return "ImageWidth";
        case TAG_IMAGE_HEIGHT:       return "ImageHeight";
        case TAG_GPS_LATITUDE_REF:   return "GPSLatitudeRef";
        case TAG_GPS_LONGITUDE_REF:  return "GPSLongitudeRef";
        case TAG_GPS_ALTITUDE:       return "GPSAltitude";
        default:                     return nullptr;
    }
}

static void parse_ifd(const uint8_t* tiff_base, size_t tiff_len,
                      uint32_t ifd_offset, int big_endian,
                      ICExifResult* result, int depth) {
    if (depth > 2) return; /* prevent infinite recursion */
    if (ifd_offset + 2 > tiff_len) return;

    uint16_t entry_count = read_u16(tiff_base + ifd_offset, big_endian);
    uint32_t pos = ifd_offset + 2;

    for (uint16_t i = 0; i < entry_count && result->count < IC_MAX_EXIF_ENTRIES; i++) {
        if (pos + 12 > tiff_len) break;

        uint16_t tag = read_u16(tiff_base + pos, big_endian);
        uint16_t type = read_u16(tiff_base + pos + 2, big_endian);
        uint32_t count = read_u32(tiff_base + pos + 4, big_endian);
        uint32_t value_offset = read_u32(tiff_base + pos + 8, big_endian);

        /* Follow EXIF sub-IFD and GPS IFD pointers */
        if (tag == TAG_EXIF_IFD_POINTER || tag == TAG_GPS_IFD_POINTER) {
            parse_ifd(tiff_base, tiff_len, value_offset, big_endian, result, depth + 1);
            pos += 12;
            continue;
        }

        const char* name = tag_name(tag);
        if (!name) { pos += 12; continue; } /* skip unknown tags */

        ICExifEntry* entry = &result->entries[result->count];
        entry->tag = tag;
        strncpy(entry->name, name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';

        /* Type sizes: 1=BYTE, 2=ASCII, 3=SHORT, 4=LONG, 5=RATIONAL,
           7=UNDEFINED, 9=SLONG, 10=SRATIONAL */
        static const int type_sizes[] = { 0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8 };
        int type_size = (type <= 10) ? type_sizes[type] : 0;
        size_t data_size = (size_t)count * type_size;

        const uint8_t* val_ptr;
        if (data_size <= 4) {
            val_ptr = tiff_base + pos + 8; /* value inline */
        } else {
            if (value_offset + data_size > tiff_len) { pos += 12; continue; }
            val_ptr = tiff_base + value_offset;
        }

        /* Format value based on type */
        switch (type) {
            case 2: /* ASCII */
                if (count > 0 && count < IC_EXIF_VALUE_LEN) {
                    memcpy(entry->value, val_ptr, count);
                    entry->value[count - 1] = '\0'; /* strip null terminator */
                }
                break;
            case 3: /* SHORT */
                snprintf(entry->value, IC_EXIF_VALUE_LEN, "%u",
                         read_u16(val_ptr, big_endian));
                break;
            case 4: /* LONG */
                snprintf(entry->value, IC_EXIF_VALUE_LEN, "%u",
                         read_u32(val_ptr, big_endian));
                break;
            case 5: { /* RATIONAL (num/denom) */
                uint32_t num = read_u32(val_ptr, big_endian);
                uint32_t den = read_u32(val_ptr + 4, big_endian);
                if (den != 0)
                    snprintf(entry->value, IC_EXIF_VALUE_LEN, "%.4f",
                             (double)num / den);
                else
                    snprintf(entry->value, IC_EXIF_VALUE_LEN, "%u/0", num);
                break;
            }
            default:
                snprintf(entry->value, IC_EXIF_VALUE_LEN, "(type %u)", type);
                break;
        }

        result->count++;
        pos += 12;
    }
}

/* ── Public API: read EXIF from raw image data ───────────────────────── */

/*
 * ic_read_exif_internal: Parse EXIF from a JPEG buffer.
 * Returns an ICExifResult with parsed tag name-value pairs.
 * Used by the JSI/WASM bindings to build the ExifData object.
 */
extern "C" int ic_read_exif_internal(const uint8_t* data, size_t len,
                                     ICExifResult* result) {
    if (!data || len < 4 || !result) return IC_ERROR_INVALID_INPUT;

    result->count = 0;

    /* Only JPEG supported for now */
    if (data[0] != 0xFF || data[1] != 0xD8)
        return IC_ERROR_UNSUPPORTED_FORMAT;

    /* Find APP1/Exif marker */
    size_t pos = 2;
    while (pos < len - 1) {
        if (data[pos] != 0xFF) break;

        uint8_t marker = data[pos + 1];

        /* SOS — stop searching */
        if (marker == 0xDA) break;

        /* Skip markers without length */
        if (marker == 0x00 || marker == 0x01 ||
            (marker >= 0xD0 && marker <= 0xD7)) {
            pos += 2;
            continue;
        }

        if (marker == 0xD9) break; /* EOI */

        if (pos + 3 >= len) break;

        uint16_t seg_len = (uint16_t)((data[pos + 2] << 8) | data[pos + 3]);
        size_t total_seg = 2 + seg_len;

        /* APP1 with Exif header */
        if (marker == 0xE1 && seg_len >= 8 &&
            pos + 4 + 6 <= len &&
            memcmp(data + pos + 4, "Exif\0\0", 6) == 0) {

            const uint8_t* tiff_base = data + pos + 4 + 6;
            size_t tiff_len = seg_len - 8; /* segment len - "Exif\0\0" - len field */

            if (tiff_len < 8) return IC_ERROR_EXIF_NOT_FOUND;

            /* Byte order: "II" = little-endian, "MM" = big-endian */
            int big_endian;
            if (tiff_base[0] == 'M' && tiff_base[1] == 'M') {
                big_endian = 1;
            } else if (tiff_base[0] == 'I' && tiff_base[1] == 'I') {
                big_endian = 0;
            } else {
                return IC_ERROR_EXIF_NOT_FOUND;
            }

            /* Verify TIFF magic (42) */
            if (read_u16(tiff_base + 2, big_endian) != 42)
                return IC_ERROR_EXIF_NOT_FOUND;

            /* IFD0 offset */
            uint32_t ifd0_offset = read_u32(tiff_base + 4, big_endian);
            parse_ifd(tiff_base, tiff_len, ifd0_offset, big_endian, result, 0);

            return IC_OK;
        }

        pos += total_seg;
    }

    return IC_ERROR_EXIF_NOT_FOUND;
}

/* ── Public API: raw EXIF (TIFF structure) extraction ────────────────── */

extern "C" int avif_get_exif(const uint8_t* data, size_t len,
                             uint8_t** out_data, size_t* out_len);
extern "C" int heif_codec_get_exif(const uint8_t* data, size_t len,
                                   uint8_t** out_data, size_t* out_len);

static int is_tiff_header(const uint8_t* p, size_t n) {
    return n >= 8 &&
           ((p[0] == 'I' && p[1] == 'I' && p[2] == 0x2A && p[3] == 0x00) ||
            (p[0] == 'M' && p[1] == 'M' && p[2] == 0x00 && p[3] == 0x2A));
}

/*
 * Copy an EXIF payload into a malloc'd buffer, skipping an optional
 * "Exif\0\0" prefix. The result must start with a TIFF header.
 */
extern "C" int ic_exif_copy_payload(const uint8_t* p, size_t n,
                                    uint8_t** out_data, size_t* out_len) {
    if (n >= 6 && memcmp(p, "Exif\0\0", 6) == 0) { p += 6; n -= 6; }
    if (!is_tiff_header(p, n)) return IC_ERROR_EXIF_NOT_FOUND;
    uint8_t* buf = static_cast<uint8_t*>(malloc(n));
    if (!buf) return IC_ERROR_ALLOC_FAILED;
    memcpy(buf, p, n);
    *out_data = buf;
    *out_len = n;
    return IC_OK;
}

static int jpeg_find_exif(const uint8_t* data, size_t len,
                          uint8_t** out_data, size_t* out_len) {
    size_t pos = 2;
    while (pos + 4 <= len) {
        if (data[pos] != 0xFF) break;
        uint8_t marker = data[pos + 1];
        if (marker == 0xFF) { pos++; continue; }          /* fill byte */
        if (marker == 0xDA || marker == 0xD9) break;      /* SOS / EOI */
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            pos += 2;
            continue;
        }
        size_t seg_len = (size_t)((data[pos + 2] << 8) | data[pos + 3]);
        if (seg_len < 2 || pos + 2 + seg_len > len) break;
        if (marker == 0xE1 && seg_len >= 8 &&
            memcmp(data + pos + 4, "Exif\0\0", 6) == 0) {
            int rc = ic_exif_copy_payload(data + pos + 4, seg_len - 2,
                                          out_data, out_len);
            /* An empty/invalid Exif APP1 (some editors leave one) shouldn't hide
             * a valid one later in the file: keep scanning. */
            if (rc != IC_ERROR_EXIF_NOT_FOUND) return rc;
        }
        pos += 2 + seg_len;
    }
    return IC_ERROR_EXIF_NOT_FOUND;
}

static int png_find_exif(const uint8_t* data, size_t len,
                         uint8_t** out_data, size_t* out_len) {
    size_t pos = 8;
    while (pos + 12 <= len) {
        uint32_t clen = read_u32(data + pos, 1);
        if (clen > len - pos - 12) break;
        const uint8_t* type = data + pos + 4;
        if (memcmp(type, "eXIf", 4) == 0)
            return ic_exif_copy_payload(data + pos + 8, clen, out_data, out_len);
        if (memcmp(type, "IDAT", 4) == 0 || memcmp(type, "IEND", 4) == 0) {
            /* eXIf may legally follow IDAT; keep scanning until IEND */
            if (type[1] == 'E') break;
        }
        pos += 12 + clen;
    }
    return IC_ERROR_EXIF_NOT_FOUND;
}

static int webp_find_exif(const uint8_t* data, size_t len,
                          uint8_t** out_data, size_t* out_len) {
    size_t pos = 12;
    while (pos + 8 <= len) {
        uint32_t clen = read_u32(data + pos + 4, 0);
        if (clen > len - pos - 8) break;
        if (memcmp(data + pos, "EXIF", 4) == 0)
            return ic_exif_copy_payload(data + pos + 8, clen, out_data, out_len);
        pos += 8 + clen + (clen & 1);
    }
    return IC_ERROR_EXIF_NOT_FOUND;
}

/*
 * ic_get_exif: return the raw EXIF TIFF structure (starting at "II*\0" or
 * "MM\0*", no "Exif\0\0" prefix). For TIFF input the whole file is returned
 * (it *is* a TIFF structure whose IFD0 carries the tags).
 */
extern "C" int ic_get_exif(const uint8_t* data, size_t len,
                           uint8_t** out_data, size_t* out_len) {
    if (!data || !len || !out_data || !out_len) return IC_ERROR_INVALID_INPUT;
    *out_data = nullptr;
    *out_len = 0;

    switch (ic_detect_format(data, len)) {
        case IC_FORMAT_JPEG: return jpeg_find_exif(data, len, out_data, out_len);
        case IC_FORMAT_PNG:  return png_find_exif(data, len, out_data, out_len);
        case IC_FORMAT_WEBP: return webp_find_exif(data, len, out_data, out_len);
        case IC_FORMAT_TIFF: return ic_exif_copy_payload(data, len, out_data, out_len);
        case IC_FORMAT_AVIF: return avif_get_exif(data, len, out_data, out_len);
        case IC_FORMAT_HEIC: return heif_codec_get_exif(data, len, out_data, out_len);
        case IC_FORMAT_UNKNOWN: return IC_ERROR_UNSUPPORTED_FORMAT;
        default: return IC_ERROR_EXIF_NOT_FOUND;
    }
}
