#include "../imagecore.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

static inline int clamp_i(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline double lanczos3(double x) {
    if (x == 0.0) return 1.0;
    if (x < -3.0 || x > 3.0) return 0.0;
    double pi_x = 3.14159265358979323846 * x;
    return (sin(pi_x) / pi_x) * (sin(pi_x / 3.0) / (pi_x / 3.0));
}

static void resize_nearest(const ICImage* img, ICImage* out,
                            double x_ratio, double y_ratio) {
    for (uint32_t dy = 0; dy < out->height; dy++) {
        for (uint32_t dx = 0; dx < out->width; dx++) {
            uint32_t sx = static_cast<uint32_t>(dx * x_ratio);
            uint32_t sy = static_cast<uint32_t>(dy * y_ratio);
            if (sx >= img->width) sx = img->width - 1;
            if (sy >= img->height) sy = img->height - 1;

            const uint8_t* src = img->pixels + sy * img->stride + sx * 4;
            uint8_t* dst = out->pixels + dy * out->stride + dx * 4;
            memcpy(dst, src, 4);
        }
    }
}

static void resize_bilinear(const ICImage* img, ICImage* out,
                             double x_ratio, double y_ratio) {
    for (uint32_t dy = 0; dy < out->height; dy++) {
        for (uint32_t dx = 0; dx < out->width; dx++) {
            double gx = dx * x_ratio;
            double gy = dy * y_ratio;
            int x0 = clamp_i(static_cast<int>(gx), 0, (int)img->width - 1);
            int y0 = clamp_i(static_cast<int>(gy), 0, (int)img->height - 1);
            int x1 = clamp_i(x0 + 1, 0, (int)img->width - 1);
            int y1 = clamp_i(y0 + 1, 0, (int)img->height - 1);

            double fx = gx - static_cast<int>(gx);
            double fy = gy - static_cast<int>(gy);

            const uint8_t* p00 = img->pixels + y0 * img->stride + x0 * 4;
            const uint8_t* p10 = img->pixels + y0 * img->stride + x1 * 4;
            const uint8_t* p01 = img->pixels + y1 * img->stride + x0 * 4;
            const uint8_t* p11 = img->pixels + y1 * img->stride + x1 * 4;

            uint8_t* dst = out->pixels + dy * out->stride + dx * 4;
            for (int c = 0; c < 4; c++) {
                double v = p00[c] * (1 - fx) * (1 - fy)
                         + p10[c] * fx * (1 - fy)
                         + p01[c] * (1 - fx) * fy
                         + p11[c] * fx * fy;
                dst[c] = static_cast<uint8_t>(clamp_i(static_cast<int>(v + 0.5), 0, 255));
            }
        }
    }
}

static void resize_lanczos(const ICImage* img, ICImage* out,
                            double x_ratio, double y_ratio) {
    for (uint32_t dy = 0; dy < out->height; dy++) {
        for (uint32_t dx = 0; dx < out->width; dx++) {
            double gx = dx * x_ratio;
            double gy = dy * y_ratio;
            int cx = static_cast<int>(gx);
            int cy = static_cast<int>(gy);

            double sum[4] = { 0, 0, 0, 0 };
            double weight_sum = 0;

            for (int ky = -2; ky <= 3; ky++) {
                int sy = clamp_i(cy + ky, 0, (int)img->height - 1);
                double wy = lanczos3(gy - (cy + ky));
                for (int kx = -2; kx <= 3; kx++) {
                    int sx = clamp_i(cx + kx, 0, (int)img->width - 1);
                    double wx = lanczos3(gx - (cx + kx));
                    double w = wx * wy;
                    const uint8_t* src = img->pixels + sy * img->stride + sx * 4;
                    for (int c = 0; c < 4; c++) {
                        sum[c] += src[c] * w;
                    }
                    weight_sum += w;
                }
            }

            uint8_t* dst = out->pixels + dy * out->stride + dx * 4;
            if (weight_sum > 0) {
                for (int c = 0; c < 4; c++) {
                    dst[c] = static_cast<uint8_t>(
                        clamp_i(static_cast<int>(sum[c] / weight_sum + 0.5), 0, 255));
                }
            } else {
                memset(dst, 0, 4);
            }
        }
    }
}

int ic_resize(const ICImage* img, uint32_t new_w, uint32_t new_h,
              ICResizeFilter filter, ICImage* out) {
    if (!img || !img->pixels || !out || new_w == 0 || new_h == 0)
        return IC_ERROR_INVALID_INPUT;

    out->width = new_w;
    out->height = new_h;
    out->stride = new_w * 4;

    size_t total = (size_t)out->stride * new_h;
    out->pixels = static_cast<uint8_t*>(malloc(total));
    if (!out->pixels) return IC_ERROR_ALLOC_FAILED;

    double x_ratio = (double)img->width / new_w;
    double y_ratio = (double)img->height / new_h;

    switch (filter) {
        case IC_FILTER_NEAREST:
            resize_nearest(img, out, x_ratio, y_ratio);
            break;
        case IC_FILTER_BILINEAR:
            resize_bilinear(img, out, x_ratio, y_ratio);
            break;
        default:
            resize_lanczos(img, out, x_ratio, y_ratio);
            break;
    }

    return IC_OK;
}
