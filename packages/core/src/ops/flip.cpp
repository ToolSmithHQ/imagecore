#include "../imagecore.h"

#include <cstdlib>
#include <cstring>

int ic_flip_horizontal(ICImage* img) {
    if (!img || !img->pixels)
        return IC_ERROR_INVALID_INPUT;

    for (uint32_t y = 0; y < img->height; y++) {
        uint8_t* row = img->pixels + y * img->stride;
        for (uint32_t x = 0; x < img->width / 2; x++) {
            uint32_t mirror_x = img->width - 1 - x;
            uint8_t tmp[4];
            memcpy(tmp, row + x * 4, 4);
            memcpy(row + x * 4, row + mirror_x * 4, 4);
            memcpy(row + mirror_x * 4, tmp, 4);
        }
    }

    return IC_OK;
}

int ic_flip_vertical(ICImage* img) {
    if (!img || !img->pixels)
        return IC_ERROR_INVALID_INPUT;

    size_t row_bytes = img->width * 4;
    uint8_t* tmp_row = static_cast<uint8_t*>(malloc(row_bytes));
    if (!tmp_row) return IC_ERROR_ALLOC_FAILED;

    for (uint32_t y = 0; y < img->height / 2; y++) {
        uint8_t* top = img->pixels + y * img->stride;
        uint8_t* bot = img->pixels + (img->height - 1 - y) * img->stride;
        memcpy(tmp_row, top, row_bytes);
        memcpy(top, bot, row_bytes);
        memcpy(bot, tmp_row, row_bytes);
    }

    free(tmp_row);
    return IC_OK;
}
