#include "../imagecore.h"

#include <cstdlib>
#include <cstring>

int ic_crop(const ICImage* img, const ICCropRegion* region, ICImage* out) {
    if (!img || !img->pixels || !region || !out)
        return IC_ERROR_INVALID_INPUT;

    if (region->x + region->width > img->width ||
        region->y + region->height > img->height ||
        region->width == 0 || region->height == 0)
        return IC_ERROR_INVALID_CROP;

    out->width = region->width;
    out->height = region->height;
    out->stride = region->width * 4;

    size_t total = (size_t)out->stride * out->height;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) return IC_ERROR_ALLOC_FAILED;

    for (uint32_t y = 0; y < region->height; y++) {
        const uint8_t* src_row = img->pixels +
            (region->y + y) * img->stride + region->x * 4;
        uint8_t* dst_row = out->pixels + y * out->stride;
        memcpy(dst_row, src_row, region->width * 4);
    }

    return IC_OK;
}
