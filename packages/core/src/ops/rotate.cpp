#include "../imagecore.h"

#include <cstdlib>
#include <cstring>

int ic_rotate(const ICImage* img, ICRotation rotation, ICImage* out) {
    if (!img || !img->pixels || !out)
        return IC_ERROR_INVALID_INPUT;

    uint32_t src_w = img->width;
    uint32_t src_h = img->height;

    switch (rotation) {
        case IC_ROTATE_90:
        case IC_ROTATE_270:
            out->width = src_h;
            out->height = src_w;
            break;
        case IC_ROTATE_180:
            out->width = src_w;
            out->height = src_h;
            break;
        default:
            return IC_ERROR_INVALID_ROTATION;
    }

    out->stride = out->width * 4;
    size_t total = (size_t)out->stride * out->height;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) return IC_ERROR_ALLOC_FAILED;

    for (uint32_t sy = 0; sy < src_h; sy++) {
        for (uint32_t sx = 0; sx < src_w; sx++) {
            const uint8_t* src_px = img->pixels + sy * img->stride + sx * 4;
            uint32_t dx, dy;

            switch (rotation) {
                case IC_ROTATE_90:
                    dx = src_h - 1 - sy;
                    dy = sx;
                    break;
                case IC_ROTATE_180:
                    dx = src_w - 1 - sx;
                    dy = src_h - 1 - sy;
                    break;
                case IC_ROTATE_270:
                    dx = sy;
                    dy = src_w - 1 - sx;
                    break;
                default:
                    dx = sx;
                    dy = sy;
                    break;
            }

            uint8_t* dst_px = out->pixels + dy * out->stride + dx * 4;
            memcpy(dst_px, src_px, 4);
        }
    }

    return IC_OK;
}
