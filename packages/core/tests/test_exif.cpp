#include "test_harness.h"

/* Minimal big-endian TIFF/EXIF block: IFD0 with Make="TestCam". */
static const uint8_t kTiff[] = {
    'M', 'M', 0x00, 0x2A, 0, 0, 0, 8,           /* header, IFD0 @ 8 */
    0, 1,                                       /* 1 entry */
    0x01, 0x0F, 0, 2, 0, 0, 0, 8, 0, 0, 0, 26,  /* Make, ASCII, 8 @ 26 */
    0, 0, 0, 0,                                 /* next IFD */
    'T', 'e', 's', 't', 'C', 'a', 'm', 0,
};

static bool exif_matches(const uint8_t* out, size_t out_len) {
    return out_len == sizeof(kTiff) && memcmp(out, kTiff, sizeof(kTiff)) == 0;
}

TEST(get_exif_jpeg_app1) {
    ICImage img = make_test_image(16, 16);
    uint8_t* jpg = nullptr; size_t jpg_len = 0;
    ICEncodeOpts opts = { 0.9f, 0, 0 };
    ASSERT(ic_encode(&img, IC_FORMAT_JPEG, &opts, &jpg, &jpg_len) == IC_OK);
    ic_image_free(&img);

    /* No EXIF yet */
    uint8_t* out = nullptr; size_t out_len = 0;
    ASSERT(ic_get_exif(jpg, jpg_len, &out, &out_len) == IC_ERROR_EXIF_NOT_FOUND);

    /* Insert APP1 "Exif\0\0" + TIFF right after SOI */
    size_t seg = 2 + 6 + sizeof(kTiff);
    size_t len = jpg_len + 2 + seg;
    uint8_t* buf = static_cast<uint8_t*>(malloc(len));
    memcpy(buf, jpg, 2);
    buf[2] = 0xFF; buf[3] = 0xE1;
    buf[4] = (uint8_t)(seg >> 8); buf[5] = (uint8_t)seg;
    memcpy(buf + 6, "Exif\0\0", 6);
    memcpy(buf + 12, kTiff, sizeof(kTiff));
    memcpy(buf + 12 + sizeof(kTiff), jpg + 2, jpg_len - 2);
    ic_free(jpg);

    ASSERT(ic_get_exif(buf, len, &out, &out_len) == IC_OK);
    ASSERT(exif_matches(out, out_len));
    ic_free(out);
    free(buf);
}

TEST(get_exif_png_exif_chunk) {
    ICImage img = make_test_image(16, 16);
    uint8_t* png = nullptr; size_t png_len = 0;
    ASSERT(ic_encode(&img, IC_FORMAT_PNG, nullptr, &png, &png_len) == IC_OK);
    ic_image_free(&img);

    /* Insert eXIf chunk after IHDR (8-byte signature + 25-byte IHDR).
       CRC is not validated by the extractor, so leave it zero. */
    size_t chunk = 12 + sizeof(kTiff);
    size_t len = png_len + chunk;
    uint8_t* buf = static_cast<uint8_t*>(calloc(1, len));
    memcpy(buf, png, 33);
    uint8_t* c = buf + 33;
    c[0] = 0; c[1] = 0; c[2] = 0; c[3] = (uint8_t)sizeof(kTiff);
    memcpy(c + 4, "eXIf", 4);
    memcpy(c + 8, kTiff, sizeof(kTiff));
    memcpy(buf + 33 + chunk, png + 33, png_len - 33);
    ic_free(png);

    uint8_t* out = nullptr; size_t out_len = 0;
    ASSERT(ic_get_exif(buf, len, &out, &out_len) == IC_OK);
    ASSERT(exif_matches(out, out_len));
    ic_free(out);
    free(buf);
}

TEST(get_exif_invalid_input) {
    uint8_t* out = nullptr; size_t out_len = 0;
    ASSERT(ic_get_exif(nullptr, 0, &out, &out_len) == IC_ERROR_INVALID_INPUT);
    uint8_t junk[16] = { 1, 2, 3, 4 };
    ASSERT(ic_get_exif(junk, sizeof(junk), &out, &out_len) == IC_ERROR_UNSUPPORTED_FORMAT);
}
