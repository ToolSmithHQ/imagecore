/*
 * Internal orientation helpers shared by the codecs and imagecore.cpp
 * (not part of the public API; see orientation.cpp).
 */
#ifndef IC_ORIENTATION_H
#define IC_ORIENTATION_H

#include "../imagecore.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Orientation (1–8) equivalent to applying `first`, then `then`. */
int ic_orientation_compose(int first, int then);

/* EXIF Orientation (1–8) from a raw EXIF block (IFD0); 1 if absent/invalid. */
int ic_exif_orientation(const uint8_t* exif, size_t len);

/* In place: Orientation -> 1 and (if set_dims) PixelX/YDimension -> width/height.
 * `exif` may start with "Exif\0\0". */
int ic_exif_patch_for_output(uint8_t* exif, size_t len, int set_dims,
                             uint32_t width, uint32_t height);

/* Writes a 26-byte big-endian TIFF block holding only IFD0 Orientation. */
size_t ic_exif_orientation_only(int orientation, uint8_t out[26]);

/* Replace img's pixels with the displayed image for orientation o (1–8). */
int ic_orient_image(ICImage* img, int o);

/* Map a displayed-coordinates rectangle (in bounds) to stored coordinates. */
void ic_orient_region_to_stored(int o, uint32_t stored_w, uint32_t stored_h,
                                const ICCropRegion* displayed, ICCropRegion* stored);

#ifdef __cplusplus
}
#endif

#endif /* IC_ORIENTATION_H */
