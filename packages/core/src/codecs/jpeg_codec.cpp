#include "../imagecore.h"
#include "jpeg_common.h"

#include <cstdlib>
#include <cstring>

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int jpeg_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    struct jpeg_decompress_struct cinfo;
    ic_jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ic_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return IC_ERROR_DECODE_FAILED;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, static_cast<unsigned long>(len));
    jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xFFFF);
    jpeg_read_header(&cinfo, TRUE);

    out->width = cinfo.image_width;
    out->height = cinfo.image_height;
    out->format = IC_FORMAT_JPEG;

    out->has_exif = 0;
    for (jpeg_saved_marker_ptr m = cinfo.marker_list; m; m = m->next) {
        if (m->marker == JPEG_APP0 + 1 && m->data_length >= 6 &&
            memcmp(m->data, "Exif\0\0", 6) == 0) {
            out->has_exif = 1;
            break;
        }
    }

    jpeg_destroy_decompress(&cinfo);
    return IC_OK;
}

/* ── Decode ──────────────────────────────────────────────────────────── */

extern "C" int jpeg_decode(const uint8_t* data, size_t len, ICImage* out) {
    struct jpeg_decompress_struct cinfo;
    ic_jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ic_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return IC_ERROR_DECODE_FAILED;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, static_cast<unsigned long>(len));
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_EXT_RGBA;
    jpeg_start_decompress(&cinfo);

    out->width = cinfo.output_width;
    out->height = cinfo.output_height;
    out->stride = cinfo.output_width * 4;

    size_t total = (size_t)out->stride * out->height;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) {
        jpeg_destroy_decompress(&cinfo);
        return IC_ERROR_ALLOC_FAILED;
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        uint8_t* row = out->pixels + cinfo.output_scanline * out->stride;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return IC_OK;
}

/* ── Encode ──────────────────────────────────────────────────────────── */

/* APP1 payload is at most 65533 bytes (65535 minus the length field);
 * "Exif\0\0" takes 6 of them. */
#define IC_JPEG_MAX_EXIF (65533 - 6)

extern "C" int jpeg_encode(const ICImage* img, const ICEncodeOpts* opts,
                           const uint8_t* exif, size_t exif_len,
                           uint8_t** out_data, size_t* out_len) {
    struct jpeg_compress_struct cinfo;
    ic_jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ic_jpeg_error_exit;

    unsigned char* buf = nullptr;
    unsigned long buf_size = 0;

    /* EXIF APP1 payload, built up front. Blocks that don't fit one APP1
     * segment are dropped (documented in imagecore.h). */
    uint8_t* volatile app1 = nullptr; /* volatile: read after longjmp */
    if (exif && exif_len > 0 && exif_len <= IC_JPEG_MAX_EXIF) {
        app1 = static_cast<uint8_t*>(malloc(exif_len + 6));
        if (!app1) return IC_ERROR_ALLOC_FAILED;
        memcpy(app1, "Exif\0\0", 6);
        memcpy(app1 + 6, exif, exif_len);
    }

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_compress(&cinfo);
        free(app1);
        return IC_ERROR_ENCODE_FAILED;
    }

    jpeg_create_compress(&cinfo);

    jpeg_mem_dest(&cinfo, &buf, &buf_size);

    cinfo.image_width = img->width;
    cinfo.image_height = img->height;
    cinfo.input_components = 4;
    cinfo.in_color_space = JCS_EXT_RGBA;

    jpeg_set_defaults(&cinfo);

    int quality = static_cast<int>(opts->quality * 100.0f);
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    /* EXIF APP1 goes right after the JFIF APP0 libjpeg emits after SOI. */
    if (app1) {
        jpeg_write_marker(&cinfo, JPEG_APP0 + 1, app1,
                          static_cast<unsigned int>(exif_len + 6));
        free(app1);
        app1 = nullptr;
    }

    while (cinfo.next_scanline < cinfo.image_height) {
        uint8_t* row = img->pixels + cinfo.next_scanline * img->stride;
        jpeg_write_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    *out_data = buf;
    *out_len = static_cast<size_t>(buf_size);
    return IC_OK;
}
