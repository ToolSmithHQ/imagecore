#include "../imagecore.h"

#include <cstdlib>
#include <cstring>
#include <png.h>

/* ── Memory read/write callbacks for libpng ──────────────────────────── */

struct ic_png_read_state {
    const uint8_t* data;
    size_t len;
    size_t offset;
};

static void ic_png_read_fn(png_structp png, png_bytep out, png_size_t count) {
    auto* state = static_cast<ic_png_read_state*>(png_get_io_ptr(png));
    if (state->offset + count > state->len) {
        png_error(png, "Read past end of buffer");
        return;
    }
    memcpy(out, state->data + state->offset, count);
    state->offset += count;
}

struct ic_png_write_state {
    uint8_t* data;
    size_t len;
    size_t capacity;
};

static void ic_png_write_fn(png_structp png, png_bytep in, png_size_t count) {
    auto* state = static_cast<ic_png_write_state*>(png_get_io_ptr(png));
    size_t needed = state->len + count;
    if (needed > state->capacity) {
        size_t new_cap = state->capacity * 2;
        if (new_cap < needed) new_cap = needed;
        uint8_t* tmp = static_cast<uint8_t*>(realloc(state->data, new_cap));
        if (!tmp) {
            png_error(png, "Allocation failed");
            return;
        }
        state->data = tmp;
        state->capacity = new_cap;
    }
    memcpy(state->data + state->len, in, count);
    state->len += count;
}

static void ic_png_flush_fn(png_structp png) {
    (void)png;
}

/* ── Get info ────────────────────────────────────────────────────────── */

extern "C" int png_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    if (len < 8 || png_sig_cmp(data, 0, 8)) {
        return IC_ERROR_INVALID_INPUT;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             nullptr, nullptr, nullptr);
    if (!png) return IC_ERROR_ALLOC_FAILED;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return IC_ERROR_ALLOC_FAILED;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return IC_ERROR_DECODE_FAILED;
    }

    ic_png_read_state state = { data, len, 0 };
    png_set_read_fn(png, &state, ic_png_read_fn);
    png_read_info(png, info);

    out->width = png_get_image_width(png, info);
    out->height = png_get_image_height(png, info);
    out->format = IC_FORMAT_PNG;

    /* Check for eXIf chunk (PNG 1.6.31+) or tEXt chunks with EXIF */
    out->has_exif = 0;
#ifdef PNG_eXIf_SUPPORTED
    png_bytep exif_data;
    png_uint_32 exif_len;
    if (png_get_eXIf_1(png, info, &exif_len, &exif_data) == PNG_INFO_eXIf) {
        out->has_exif = 1;
    }
#endif

    png_destroy_read_struct(&png, &info, nullptr);
    return IC_OK;
}

/* ── Decode ──────────────────────────────────────────────────────────── */

extern "C" int png_decode(const uint8_t* data, size_t len, ICImage* out) {
    if (len < 8 || png_sig_cmp(data, 0, 8)) {
        return IC_ERROR_INVALID_INPUT;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             nullptr, nullptr, nullptr);
    if (!png) return IC_ERROR_ALLOC_FAILED;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return IC_ERROR_ALLOC_FAILED;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return IC_ERROR_DECODE_FAILED;
    }

    ic_png_read_state state = { data, len, 0 };
    png_set_read_fn(png, &state, ic_png_read_fn);

    png_read_info(png, info);

    uint32_t width = png_get_image_width(png, info);
    uint32_t height = png_get_image_height(png, info);
    int bit_depth = png_get_bit_depth(png, info);
    int color_type = png_get_color_type(png, info);

    /* Transform to RGBA 8-bit */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (bit_depth == 16)
        png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    if (!(color_type & PNG_COLOR_MASK_ALPHA))
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    out->width = width;
    out->height = height;
    out->stride = width * 4;

    size_t total = (size_t)out->stride * height;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) {
        png_destroy_read_struct(&png, &info, nullptr);
        return IC_ERROR_ALLOC_FAILED;
    }

    png_bytep* row_ptrs = static_cast<png_bytep*>(
        malloc(sizeof(png_bytep) * height));
    if (!row_ptrs) {
        free(out->pixels);
        out->pixels = nullptr;
        png_destroy_read_struct(&png, &info, nullptr);
        return IC_ERROR_ALLOC_FAILED;
    }

    for (uint32_t y = 0; y < height; y++) {
        row_ptrs[y] = out->pixels + y * out->stride;
    }

    png_read_image(png, row_ptrs);
    png_read_end(png, nullptr);

    free(row_ptrs);
    png_destroy_read_struct(&png, &info, nullptr);
    return IC_OK;
}

/* ── Encode ──────────────────────────────────────────────────────────── */

/*
 * zlib level for PNG output. PNG is lossless, so ICEncodeOpts::quality does
 * not apply (it used to pick the zlib level, which made q=0.85-0.9 encode at
 * level 1-2). Level 6 with libpng's adaptive per-row filter choice
 * (PNG_ALL_FILTERS) is the sweet spot: measured on photos, level 9 is only
 * ~1-2% smaller but 2-4x slower on small images and ~6-9x slower at 48 MP
 * (8000x6000: ~57 s vs ~6-9 s natively), which is unusable in the browser.
 * Overridable at build time.
 */
#ifndef IC_PNG_COMPRESSION_LEVEL
#define IC_PNG_COMPRESSION_LEVEL 6
#endif

static bool ic_png_is_opaque(const ICImage* img) {
    for (uint32_t y = 0; y < img->height; y++) {
        const uint8_t* a = img->pixels + (size_t)y * img->stride + 3;
        for (uint32_t x = 0; x < img->width; x++, a += 4)
            if (*a != 0xFF) return false;
    }
    return true;
}

/*
 * Writes 8-bit RGB (colour type 2) when every pixel is opaque, RGBA (6)
 * otherwise.
 */
extern "C" int png_encode(const ICImage* img, const ICEncodeOpts* opts,
                          uint8_t** out_data, size_t* out_len) {
    (void)opts;
    if (!img->width || !img->height || img->stride < img->width * 4)
        return IC_ERROR_INVALID_INPUT;
    const bool opaque = ic_png_is_opaque(img);

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              nullptr, nullptr, nullptr);
    if (!png) return IC_ERROR_ALLOC_FAILED;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        return IC_ERROR_ALLOC_FAILED;
    }

    /* Initial buffer: 64KB. Lives outside the setjmp scope so it can be
     * released when libpng longjmps out on error. */
    ic_png_write_state state = { nullptr, 0, 65536 };
    state.data = static_cast<uint8_t*>(malloc(state.capacity));
    if (!state.data) {
        png_destroy_write_struct(&png, &info);
        return IC_ERROR_ALLOC_FAILED;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        free(state.data);
        return IC_ERROR_ENCODE_FAILED;
    }

    png_set_write_fn(png, &state, ic_png_write_fn, ic_png_flush_fn);

    png_set_IHDR(png, info, img->width, img->height, 8,
                 opaque ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_compression_level(png, IC_PNG_COMPRESSION_LEVEL);
    png_set_filter(png, PNG_FILTER_TYPE_BASE, PNG_ALL_FILTERS);

    png_write_info(png, info);

    /* Opaque: rows are still RGBA in memory; have libpng drop the 4th byte. */
    if (opaque)
        png_set_filler(png, 0, PNG_FILLER_AFTER);

    for (uint32_t y = 0; y < img->height; y++) {
        png_bytep row = img->pixels + (size_t)y * img->stride;
        png_write_row(png, row);
    }

    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);

    *out_data = state.data;
    *out_len = state.len;
    return IC_OK;
}
