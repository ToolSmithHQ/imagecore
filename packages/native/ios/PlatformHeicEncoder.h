#ifndef IC_PLATFORM_HEIC_ENCODER_H
#define IC_PLATFORM_HEIC_ENCODER_H

#include "imagecore.h"

#ifdef __cplusplus
extern "C" {
#endif

int platform_heic_decode(const uint8_t* data, size_t len, ICImage* out);
int platform_heic_encode(const ICImage* img, const ICEncodeOpts* opts,
                         uint8_t** out_data, size_t* out_len);
int platform_heic_get_info(const uint8_t* data, size_t len, ICImageInfo* out);

#ifdef __cplusplus
}
#endif

#endif
