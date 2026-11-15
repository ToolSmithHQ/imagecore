#include "../imagecore.h"
#include "jpeg_common.h"
#include "../ops/orientation.h"

#include <cstdlib>
#include <cstring>

extern "C" {
#include "transupp.h"
}

/* ── Shared lossless transform helper ────────────────────────────────── */

/*
 * Both rotate and crop use the same libjpeg-turbo lossless transform pipeline:
 *   1. Read source coefficients
 *   2. Set up transform options
 *   3. Write transformed coefficients
 * Only the transform options differ.
 *
 * reset_orientation: the transform bakes the EXIF orientation into the
 * coefficients, so write Orientation=1 (and the new PixelX/YDimension) into
 * every copied Exif APP1 segment.
 */
static int jpeg_lossless_transform(const uint8_t* data, size_t len,
                                    jpeg_transform_info* xform,
                                    bool reset_orientation,
                                    uint8_t** out_data, size_t* out_len) {
    struct jpeg_decompress_struct srcinfo;
    struct jpeg_compress_struct dstinfo;
    ic_jpeg_error_mgr jsrcerr, jdsterr;

    /* Set up source decompressor */
    srcinfo.err = jpeg_std_error(&jsrcerr.pub);
    jsrcerr.pub.error_exit = ic_jpeg_error_exit;

    if (setjmp(jsrcerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&srcinfo);
        return IC_ERROR_DECODE_FAILED;
    }

    jpeg_create_decompress(&srcinfo);
    jpeg_mem_src(&srcinfo, data, static_cast<unsigned long>(len));
    jcopy_markers_setup(&srcinfo, JCOPYOPT_ALL);
    jpeg_read_header(&srcinfo, TRUE);

    if (!jtransform_request_workspace(&srcinfo, xform)) {
        jpeg_destroy_decompress(&srcinfo);
        return IC_ERROR_INVALID_ROTATION;
    }

    jvirt_barray_ptr* src_coefs = jpeg_read_coefficients(&srcinfo);

    /* Set up destination compressor */
    dstinfo.err = jpeg_std_error(&jdsterr.pub);
    jdsterr.pub.error_exit = ic_jpeg_error_exit;

    if (setjmp(jdsterr.setjmp_buffer)) {
        jpeg_destroy_compress(&dstinfo);
        jpeg_destroy_decompress(&srcinfo);
        return IC_ERROR_ENCODE_FAILED;
    }

    jpeg_create_compress(&dstinfo);

    unsigned char* buf = nullptr;
    unsigned long buf_size = 0;
    jpeg_mem_dest(&dstinfo, &buf, &buf_size);

    jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

    jvirt_barray_ptr* dst_coefs =
        jtransform_adjust_parameters(&srcinfo, &dstinfo, src_coefs, xform);

    if (reset_orientation) {
        for (jpeg_saved_marker_ptr m = srcinfo.marker_list; m; m = m->next) {
            if (m->marker == JPEG_APP0 + 1 && m->data_length >= 6 &&
                memcmp(m->data, "Exif\0\0", 6) == 0)
                ic_exif_patch_for_output(m->data, m->data_length, 1,
                                         dstinfo.image_width, dstinfo.image_height);
        }
    }

    jpeg_write_coefficients(&dstinfo, dst_coefs);
    jcopy_markers_execute(&srcinfo, &dstinfo, JCOPYOPT_ALL);
    jtransform_execute_transform(&srcinfo, &dstinfo, src_coefs, xform);

    jpeg_finish_compress(&dstinfo);
    jpeg_destroy_compress(&dstinfo);
    jpeg_finish_decompress(&srcinfo);
    jpeg_destroy_decompress(&srcinfo);

    *out_data = buf;
    *out_len = static_cast<size_t>(buf_size);
    return IC_OK;
}

/* ── Lossless rotation ───────────────────────────────────────────────── */

/* libjpeg-turbo transform for each orientation value (see orientation.cpp). */
static const JXFORM_CODE kXformForOrientation[9] = {
    JXFORM_NONE,
    JXFORM_NONE,       /* 1 */
    JXFORM_FLIP_H,     /* 2 */
    JXFORM_ROT_180,    /* 3 */
    JXFORM_FLIP_V,     /* 4 */
    JXFORM_TRANSPOSE,  /* 5 */
    JXFORM_ROT_90,     /* 6 */
    JXFORM_TRANSVERSE, /* 7 */
    JXFORM_ROT_270,    /* 8 */
};

/*
 * `orientation` is the source's EXIF Orientation. The rotation is relative
 * to the image as displayed, so for orientation != 1 the EXIF transform and
 * the rotation are composed into one lossless transform and the output gets
 * Orientation=1. Orientation 1 keeps the plain rotate (with edge trimming).
 */
extern "C" int jpeg_lossless_rotate(const uint8_t* data, size_t len,
                                    ICRotation rotation, int orientation,
                                    uint8_t** out_data, size_t* out_len) {
    jpeg_transform_info xform;
    memset(&xform, 0, sizeof(xform));
    xform.trim = TRUE;

    int rot;
    switch (rotation) {
        case IC_ROTATE_90:  rot = 6; break;
        case IC_ROTATE_180: rot = 3; break;
        case IC_ROTATE_270: rot = 8; break;
        default:            return IC_ERROR_INVALID_ROTATION;
    }
    bool oriented = orientation >= 2 && orientation <= 8;
    xform.transform = kXformForOrientation[oriented ? ic_orientation_compose(orientation, rot) : rot];

    return jpeg_lossless_transform(data, len, &xform, oriented, out_data, out_len);
}

/* ── Lossless crop ───────────────────────────────────────────────────── */

extern "C" int jpeg_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

/*
 * `region` is in displayed coordinates. For a source with EXIF orientation
 * != 1 it is mapped onto the stored pixels and the orientation tag is kept,
 * so the crop itself stays a pure (untransformed) lossless crop.
 */
extern "C" int jpeg_lossless_crop(const uint8_t* data, size_t len,
                                  const ICCropRegion* region, int orientation,
                                  uint8_t** out_data, size_t* out_len) {
    ICCropRegion r = *region;
    if (orientation >= 2 && orientation <= 8) {
        ICImageInfo info;
        memset(&info, 0, sizeof(info));
        int err = jpeg_get_info(data, len, &info); /* stored dimensions */
        if (err != IC_OK) return err;
        uint32_t dw = orientation >= 5 ? info.height : info.width;
        uint32_t dh = orientation >= 5 ? info.width : info.height;
        if (r.width == 0 || r.height == 0 || r.x >= dw || r.y >= dh ||
            r.width > dw - r.x || r.height > dh - r.y)
            return IC_ERROR_INVALID_CROP;
        ic_orient_region_to_stored(orientation, info.width, info.height, region, &r);
    }

    jpeg_transform_info xform;
    memset(&xform, 0, sizeof(xform));
    xform.transform = JXFORM_NONE;
    xform.trim = TRUE;
    xform.crop = TRUE;
    xform.crop_xoffset = r.x;
    xform.crop_xoffset_set = JCROP_POS;
    xform.crop_yoffset = r.y;
    xform.crop_yoffset_set = JCROP_POS;
    xform.crop_width = r.width;
    xform.crop_width_set = JCROP_POS;
    xform.crop_height = r.height;
    xform.crop_height_set = JCROP_POS;

    return jpeg_lossless_transform(data, len, &xform, false, out_data, out_len);
}

/* ── Binary EXIF strip ───────────────────────────────────────────────── */

/*
 * Single-pass marker walker: copies all JPEG segments except APP1/Exif.
 * No pixel decoding. Output is never larger than the input.
 *
 * `orientation` is the source's EXIF Orientation. When it is not 1, the
 * first Exif APP1 is replaced by a minimal one holding only Orientation, so
 * a photo stored sideways still displays upright; nothing else is kept.
 */

static bool is_exif_app1(const uint8_t* data, size_t pos, size_t len,
                          uint16_t seg_len) {
    return seg_len >= 8 && pos + 4 + 6 <= len &&
           memcmp(data + pos + 4, "Exif\0\0", 6) == 0;
}

extern "C" int jpeg_strip_exif(const uint8_t* data, size_t len, int orientation,
                               uint8_t** out_data, size_t* out_len) {
    if (len < 2 || data[0] != 0xFF || data[1] != 0xD8)
        return IC_ERROR_INVALID_INPUT;

    /* The orientation-only APP1 replaces an Exif APP1 that held the tag, so
     * the output stays <= input; the slack covers an empty Exif APP1 coming
     * before the real one. */
    bool keep_orientation = orientation >= 2 && orientation <= 8;
    uint8_t* out = static_cast<uint8_t*>(malloc(len + 64));
    if (!out) return IC_ERROR_ALLOC_FAILED;

    /* Copy SOI */
    out[0] = 0xFF;
    out[1] = 0xD8;
    size_t out_pos = 2;
    size_t pos = 2;

    while (pos < len - 1) {
        if (data[pos] != 0xFF) break;
        uint8_t marker = data[pos + 1];

        /* SOS — copy everything remaining (entropy-coded data) */
        if (marker == 0xDA) {
            size_t remaining = len - pos;
            memcpy(out + out_pos, data + pos, remaining);
            out_pos += remaining;
            break;
        }

        /* EOI */
        if (marker == 0xD9) {
            memcpy(out + out_pos, data + pos, 2);
            out_pos += 2;
            break;
        }

        /* Standalone markers (no length field) */
        if (marker == 0x00 || marker == 0x01 ||
            (marker >= 0xD0 && marker <= 0xD7)) {
            memcpy(out + out_pos, data + pos, 2);
            out_pos += 2;
            pos += 2;
            continue;
        }

        /* Marker with length field */
        if (pos + 3 >= len) break;
        uint16_t seg_len = static_cast<uint16_t>((data[pos + 2] << 8) | data[pos + 3]);
        size_t total_seg = 2 + seg_len;
        if (seg_len < 2 || total_seg > len - pos) break; /* truncated segment */

        /* Skip APP1/Exif, copy everything else */
        if (marker == (JPEG_APP0 + 1) && is_exif_app1(data, pos, len, seg_len)) {
            if (keep_orientation) {
                /* FF E1, length, "Exif\0\0", 26-byte TIFF block */
                uint8_t* p = out + out_pos;
                size_t tiff_len = ic_exif_orientation_only(orientation, p + 10);
                size_t seg = 2 + 6 + tiff_len;
                p[0] = 0xFF; p[1] = 0xE1;
                p[2] = (uint8_t)(seg >> 8); p[3] = (uint8_t)seg;
                memcpy(p + 4, "Exif\0\0", 6);
                out_pos += 2 + seg;
                keep_orientation = false;
            }
            pos += total_seg;
            continue;
        }

        memcpy(out + out_pos, data + pos, total_seg);
        out_pos += total_seg;
        pos += total_seg;
    }

    *out_data = out;
    *out_len = out_pos;
    return IC_OK;
}
