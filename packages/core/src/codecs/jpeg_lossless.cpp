#include "../imagecore.h"
#include "jpeg_common.h"

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
 */
static int jpeg_lossless_transform(const uint8_t* data, size_t len,
                                    jpeg_transform_info* xform,
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

extern "C" int jpeg_lossless_rotate(const uint8_t* data, size_t len,
                                    ICRotation rotation,
                                    uint8_t** out_data, size_t* out_len) {
    jpeg_transform_info xform;
    memset(&xform, 0, sizeof(xform));
    xform.trim = TRUE;

    switch (rotation) {
        case IC_ROTATE_90:  xform.transform = JXFORM_ROT_90;  break;
        case IC_ROTATE_180: xform.transform = JXFORM_ROT_180; break;
        case IC_ROTATE_270: xform.transform = JXFORM_ROT_270; break;
        default:            return IC_ERROR_INVALID_ROTATION;
    }

    return jpeg_lossless_transform(data, len, &xform, out_data, out_len);
}

/* ── Lossless crop ───────────────────────────────────────────────────── */

extern "C" int jpeg_lossless_crop(const uint8_t* data, size_t len,
                                  const ICCropRegion* region,
                                  uint8_t** out_data, size_t* out_len) {
    jpeg_transform_info xform;
    memset(&xform, 0, sizeof(xform));
    xform.transform = JXFORM_NONE;
    xform.trim = TRUE;
    xform.crop = TRUE;
    xform.crop_xoffset = region->x;
    xform.crop_xoffset_set = JCROP_POS;
    xform.crop_yoffset = region->y;
    xform.crop_yoffset_set = JCROP_POS;
    xform.crop_width = region->width;
    xform.crop_width_set = JCROP_POS;
    xform.crop_height = region->height;
    xform.crop_height_set = JCROP_POS;

    return jpeg_lossless_transform(data, len, &xform, out_data, out_len);
}

/* ── Binary EXIF strip ───────────────────────────────────────────────── */

/*
 * Single-pass marker walker: copies all JPEG segments except APP1/Exif.
 * No pixel decoding. Output is always <= input size.
 */

static bool is_exif_app1(const uint8_t* data, size_t pos, size_t len,
                          uint16_t seg_len) {
    return seg_len >= 8 && pos + 4 + 6 <= len &&
           memcmp(data + pos + 4, "Exif\0\0", 6) == 0;
}

extern "C" int jpeg_strip_exif(const uint8_t* data, size_t len,
                               uint8_t** out_data, size_t* out_len) {
    if (len < 2 || data[0] != 0xFF || data[1] != 0xD8)
        return IC_ERROR_INVALID_INPUT;

    /* Allocate output buffer same size as input (output will be <= input) */
    uint8_t* out = static_cast<uint8_t*>(malloc(len));
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

        /* Skip APP1/Exif, copy everything else */
        if (marker == (JPEG_APP0 + 1) && is_exif_app1(data, pos, len, seg_len)) {
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
