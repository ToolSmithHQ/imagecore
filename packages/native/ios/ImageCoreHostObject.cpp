#include "ImageCoreHostObject.h"
#include "imagecore.h"

#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

/* Forward declarations (C linkage, outside namespace) */
extern "C" {
    struct ICExifEntry { uint16_t tag; char name[32]; char value[128]; };
    struct ICExifResult { ICExifEntry entries[64]; int count; };
    int ic_read_exif_internal(const uint8_t*, size_t, ICExifResult*);

    int platform_heic_decode(const uint8_t* data, size_t len, ICImage* out);
    int platform_heic_encode(const ICImage* img, const ICEncodeOpts* opts,
                             uint8_t** out_data, size_t* out_len);
    int platform_heic_get_info(const uint8_t* data, size_t len, ICImageInfo* out);
}

/* HEIC-aware wrappers */
static int ic_decode_with_heic(const uint8_t* data, size_t len, ICImage* out) {
    if (ic_detect_format(data, len) == IC_FORMAT_HEIC)
        return platform_heic_decode(data, len, out);
    return ic_decode(data, len, out);
}

static int ic_encode_with_heic(const ICImage* img, ICFormat fmt,
                               const ICEncodeOpts* opts,
                               uint8_t** out_data, size_t* out_len) {
    if (fmt == IC_FORMAT_HEIC)
        return platform_heic_encode(img, opts, out_data, out_len);
    return ic_encode(img, fmt, opts, out_data, out_len);
}

static int ic_get_info_with_heic(const uint8_t* data, size_t len, ICImageInfo* out) {
    if (ic_detect_format(data, len) == IC_FORMAT_HEIC)
        return platform_heic_get_info(data, len, out);
    return ic_get_image_info(data, len, out);
}

namespace imagecore {

using namespace facebook;

/* ── Pointer registry (avoids double precision loss on 64-bit) ───────── */

static std::mutex g_ptrMutex;
static std::unordered_map<uint32_t, ICImage*> g_ptrMap;
static uint32_t g_nextPtrId = 1;

static uint32_t registerPtr(ICImage* ptr) {
    std::lock_guard<std::mutex> lock(g_ptrMutex);
    uint32_t id = g_nextPtrId++;
    g_ptrMap[id] = ptr;
    return id;
}

static ICImage* lookupPtr(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_ptrMutex);
    auto it = g_ptrMap.find(id);
    return (it != g_ptrMap.end()) ? it->second : nullptr;
}

static void unregisterPtr(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_ptrMutex);
    g_ptrMap.erase(id);
}

/* ── MutableBuffer for ArrayBuffer creation ──────────────────────────── */

class ICMutableBuffer : public jsi::MutableBuffer {
public:
    ICMutableBuffer(const uint8_t* src, size_t size) : size_(size) {
        data_ = static_cast<uint8_t*>(malloc(size));
        if (data_ && src) memcpy(data_, src, size);
    }
    ~ICMutableBuffer() override { free(data_); }
    size_t size() const override { return size_; }
    uint8_t* data() override { return data_; }
private:
    uint8_t* data_;
    size_t size_;
};

/* ── Image allocation helpers ────────────────────────────────────────── */

/** Allocate a new ICImage with malloc (consistent with ic_image_free). */
static ICImage* allocImage() {
    auto* img = static_cast<ICImage*>(calloc(1, sizeof(ICImage)));
    return img;
}

/** Clone an ICImage (deep copy of pixel data). */
static ICImage* cloneImage(const ICImage* src) {
    auto* copy = allocImage();
    if (!copy) return nullptr;
    copy->width = src->width;
    copy->height = src->height;
    copy->stride = src->stride;
    size_t total = static_cast<size_t>(copy->stride) * copy->height;
    copy->pixels = static_cast<uint8_t*>(malloc(total));
    if (copy->pixels) memcpy(copy->pixels, src->pixels, total);
    return copy;
}

/** Free an ICImage allocated with allocImage/cloneImage. */
static void freeImage(ICImage* img) {
    if (img) {
        ic_image_free(img);
        free(img);
    }
}

/* Forward declarations */
static ICImage* extractNativeImage(jsi::Runtime& rt, const jsi::Value& val, const char* op);
static jsi::Value wrapDecodedImage(jsi::Runtime& rt, ICImage* img);

/* ── JSI Helpers ─────────────────────────────────────────────────────── */

std::pair<const uint8_t*, size_t>
ImageCoreHostObject::getArrayBufferData(jsi::Runtime& rt, const jsi::Value& val) {
    auto obj = val.asObject(rt);
    if (!obj.isArrayBuffer(rt)) throw jsi::JSError(rt, "Expected ArrayBuffer");
    auto buf = obj.getArrayBuffer(rt);
    return { buf.data(rt), buf.size(rt) };
}

jsi::Value
ImageCoreHostObject::makeArrayBuffer(jsi::Runtime& rt, const uint8_t* data, size_t len) {
    std::shared_ptr<jsi::MutableBuffer> buffer(new ICMutableBuffer(data, len));
    return jsi::ArrayBuffer(rt, std::move(buffer));
}

static const char* formatToString(ICFormat fmt) {
    switch (fmt) {
        case IC_FORMAT_JPEG: return "jpeg"; case IC_FORMAT_PNG: return "png";
        case IC_FORMAT_WEBP: return "webp"; case IC_FORMAT_HEIC: return "heic";
        case IC_FORMAT_AVIF: return "avif"; case IC_FORMAT_TIFF: return "tiff";
        case IC_FORMAT_BMP:  return "bmp";  case IC_FORMAT_GIF:  return "gif";
        default: return "unknown";
    }
}

static ICFormat stringToFormat(const std::string& s) {
    if (s == "jpeg") return IC_FORMAT_JPEG; if (s == "png") return IC_FORMAT_PNG;
    if (s == "webp") return IC_FORMAT_WEBP; if (s == "heic") return IC_FORMAT_HEIC;
    if (s == "avif") return IC_FORMAT_AVIF; if (s == "tiff") return IC_FORMAT_TIFF;
    if (s == "bmp") return IC_FORMAT_BMP;   if (s == "gif") return IC_FORMAT_GIF;
    return IC_FORMAT_UNKNOWN;
}

jsi::Value ImageCoreHostObject::makeImageInfo(jsi::Runtime& rt, uint32_t w, uint32_t h,
                                               const char* fmt, bool hasExif, size_t fileSize) {
    auto obj = jsi::Object(rt);
    obj.setProperty(rt, "width", static_cast<int>(w));
    obj.setProperty(rt, "height", static_cast<int>(h));
    obj.setProperty(rt, "format", jsi::String::createFromAscii(rt, fmt));
    obj.setProperty(rt, "hasExif", hasExif);
    obj.setProperty(rt, "fileSize", static_cast<int>(fileSize));
    return std::move(obj);
}

static void checkError(jsi::Runtime& rt, int err, const char* op) {
    if (err == IC_OK) return;
    const char* msgs[] = { "", "Invalid input", "Unsupported format", "Decode failed",
                           "Encode failed", "Alloc failed", "Invalid rotation",
                           "Invalid crop region", "EXIF not found" };
    int idx = (-err >= 1 && -err <= 8) ? -err : 0;
    throw jsi::JSError(rt, std::string("ImageCore.") + op + ": " + msgs[idx]);
}

/* ── Decoded image helpers ───────────────────────────────────────────── */

static ICImage* extractNativeImage(jsi::Runtime& rt, const jsi::Value& val, const char* op) {
    auto obj = val.asObject(rt);
    auto id_val = obj.getProperty(rt, "_nativePtrId");
    if (id_val.isUndefined())
        throw jsi::JSError(rt, std::string(op) + ": invalid DecodedImage");
    auto img = lookupPtr(static_cast<uint32_t>(id_val.asNumber()));
    if (!img || !img->pixels)
        throw jsi::JSError(rt, std::string(op) + ": DecodedImage freed");
    return img;
}

static jsi::Value wrapDecodedImage(jsi::Runtime& rt, ICImage* img) {
    auto obj = jsi::Object(rt);
    obj.setProperty(rt, "width", static_cast<int>(img->width));
    obj.setProperty(rt, "height", static_cast<int>(img->height));
    uint32_t ptrId = registerPtr(img);
    obj.setProperty(rt, "_nativePtrId", static_cast<int>(ptrId));
    obj.setProperty(rt, "free",
        jsi::Function::createFromHostFunction(rt,
            jsi::PropNameID::forAscii(rt, "free"), 0,
            [ptrId](jsi::Runtime&, const jsi::Value&,
                   const jsi::Value*, size_t) -> jsi::Value {
                auto ptr = lookupPtr(ptrId);
                if (ptr) { freeImage(ptr); unregisterPtr(ptrId); }
                return jsi::Value::undefined();
            }));
    return std::move(obj);
}

/* ── API methods ─────────────────────────────────────────────────────── */

jsi::Value ImageCoreHostObject::getImageInfo(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 1) throw jsi::JSError(rt, "getImageInfo requires 1 argument");
    auto [data, len] = getArrayBufferData(rt, args[0]);
    ICImageInfo info;
    checkError(rt, ic_get_info_with_heic(data, len, &info), "getImageInfo");
    return makeImageInfo(rt, info.width, info.height, formatToString(info.format), info.has_exif, info.file_size);
}

jsi::Value ImageCoreHostObject::decode(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 1) throw jsi::JSError(rt, "decode requires 1 argument");
    auto [data, len] = getArrayBufferData(rt, args[0]);
    ICImage* img = allocImage();
    if (!img) throw jsi::JSError(rt, "decode: allocation failed");
    int err = ic_decode_with_heic(data, len, img);
    if (err != IC_OK) { freeImage(img); checkError(rt, err, "decode"); }
    return wrapDecodedImage(rt, img);
}

jsi::Value ImageCoreHostObject::encode(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 2) throw jsi::JSError(rt, "encode requires 2 arguments");
    auto* img = extractNativeImage(rt, args[0], "encode");
    auto opts_obj = args[1].asObject(rt);
    ICFormat fmt = stringToFormat(opts_obj.getProperty(rt, "format").asString(rt).utf8(rt));
    if (fmt == IC_FORMAT_UNKNOWN) throw jsi::JSError(rt, "encode: unknown format");
    ICEncodeOpts opts = { 0.85f, 0, 0 };
    if (opts_obj.hasProperty(rt, "quality")) opts.quality = static_cast<float>(opts_obj.getProperty(rt, "quality").asNumber());
    if (opts_obj.hasProperty(rt, "lossless")) opts.lossless = opts_obj.getProperty(rt, "lossless").getBool() ? 1 : 0;
    uint8_t* out_data = nullptr; size_t out_len = 0;
    checkError(rt, ic_encode_with_heic(img, fmt, &opts, &out_data, &out_len), "encode");
    auto result = makeArrayBuffer(rt, out_data, out_len);
    ic_free(out_data);
    return result;
}

jsi::Value ImageCoreHostObject::convert(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 2) throw jsi::JSError(rt, "convert requires 2 arguments");
    auto [data, len] = getArrayBufferData(rt, args[0]);
    auto opts_obj = args[1].asObject(rt);
    ICFormat fmt = stringToFormat(opts_obj.getProperty(rt, "format").asString(rt).utf8(rt));
    if (fmt == IC_FORMAT_UNKNOWN) throw jsi::JSError(rt, "convert: unknown format");
    ICEncodeOpts opts = { 0.85f, 0, 0 };
    if (opts_obj.hasProperty(rt, "quality")) opts.quality = static_cast<float>(opts_obj.getProperty(rt, "quality").asNumber());
    if (opts_obj.hasProperty(rt, "lossless")) opts.lossless = opts_obj.getProperty(rt, "lossless").getBool() ? 1 : 0;
    if (opts_obj.hasProperty(rt, "stripExif")) opts.strip_exif = opts_obj.getProperty(rt, "stripExif").getBool() ? 1 : 0;
    ICImage img;
    checkError(rt, ic_decode_with_heic(data, len, &img), "convert(decode)");
    uint8_t* out_data = nullptr; size_t out_len = 0;
    int err = ic_encode_with_heic(&img, fmt, &opts, &out_data, &out_len);
    ic_image_free(&img);
    checkError(rt, err, "convert(encode)");
    auto result = makeArrayBuffer(rt, out_data, out_len);
    ic_free(out_data);
    return result;
}

jsi::Value ImageCoreHostObject::jpegLosslessRotate(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 2) throw jsi::JSError(rt, "jpegLosslessRotate requires 2 arguments");
    auto [data, len] = getArrayBufferData(rt, args[0]);
    uint8_t* out_data = nullptr; size_t out_len = 0;
    checkError(rt, ic_jpeg_lossless_rotate(data, len, static_cast<ICRotation>(static_cast<int>(args[1].asNumber())), &out_data, &out_len), "jpegLosslessRotate");
    auto result = makeArrayBuffer(rt, out_data, out_len);
    ic_free(out_data);
    return result;
}

jsi::Value ImageCoreHostObject::jpegLosslessCrop(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 2) throw jsi::JSError(rt, "jpegLosslessCrop requires 2 arguments");
    auto [data, len] = getArrayBufferData(rt, args[0]);
    auto r = args[1].asObject(rt);
    ICCropRegion region = {
        static_cast<uint32_t>(r.getProperty(rt, "x").asNumber()),
        static_cast<uint32_t>(r.getProperty(rt, "y").asNumber()),
        static_cast<uint32_t>(r.getProperty(rt, "width").asNumber()),
        static_cast<uint32_t>(r.getProperty(rt, "height").asNumber())
    };
    uint8_t* out_data = nullptr; size_t out_len = 0;
    checkError(rt, ic_jpeg_lossless_crop(data, len, &region, &out_data, &out_len), "jpegLosslessCrop");
    auto result = makeArrayBuffer(rt, out_data, out_len);
    ic_free(out_data);
    return result;
}

jsi::Value ImageCoreHostObject::jpegStripExif(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 1) throw jsi::JSError(rt, "jpegStripExif requires 1 argument");
    auto [data, len] = getArrayBufferData(rt, args[0]);
    uint8_t* out_data = nullptr; size_t out_len = 0;
    checkError(rt, ic_jpeg_strip_exif(data, len, &out_data, &out_len), "jpegStripExif");
    auto result = makeArrayBuffer(rt, out_data, out_len);
    ic_free(out_data);
    return result;
}

jsi::Value ImageCoreHostObject::stripExif(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 1) throw jsi::JSError(rt, "stripExif requires 1 argument");
    auto [data, len] = getArrayBufferData(rt, args[0]);
    ICFormat fmt = ic_detect_format(data, len);
    if (fmt == IC_FORMAT_JPEG) {
        uint8_t* out_data = nullptr; size_t out_len = 0;
        checkError(rt, ic_jpeg_strip_exif(data, len, &out_data, &out_len), "stripExif");
        auto result = makeArrayBuffer(rt, out_data, out_len);
        ic_free(out_data);
        return result;
    }
    ICImage img;
    checkError(rt, ic_decode_with_heic(data, len, &img), "stripExif(decode)");
    ICEncodeOpts opts = { 0.95f, 0, 1 };
    uint8_t* out_data = nullptr; size_t out_len = 0;
    int err = ic_encode_with_heic(&img, fmt, &opts, &out_data, &out_len);
    ic_image_free(&img);
    checkError(rt, err, "stripExif(encode)");
    auto result = makeArrayBuffer(rt, out_data, out_len);
    ic_free(out_data);
    return result;
}

jsi::Value ImageCoreHostObject::readExif(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 1) throw jsi::JSError(rt, "readExif requires 1 argument");
    auto [data, len] = getArrayBufferData(rt, args[0]);
    ICExifResult result;
    int err = ic_read_exif_internal(data, len, &result);
    if (err == IC_ERROR_EXIF_NOT_FOUND) return jsi::Object(rt);
    checkError(rt, err, "readExif");
    auto obj = jsi::Object(rt);
    for (int i = 0; i < result.count; i++)
        obj.setProperty(rt, result.entries[i].name, jsi::String::createFromUtf8(rt, result.entries[i].value));
    return obj;
}

jsi::Value ImageCoreHostObject::crop(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 2) throw jsi::JSError(rt, "crop requires 2 arguments");
    auto* src = extractNativeImage(rt, args[0], "crop");
    auto r = args[1].asObject(rt);
    ICCropRegion region = {
        static_cast<uint32_t>(r.getProperty(rt, "x").asNumber()),
        static_cast<uint32_t>(r.getProperty(rt, "y").asNumber()),
        static_cast<uint32_t>(r.getProperty(rt, "width").asNumber()),
        static_cast<uint32_t>(r.getProperty(rt, "height").asNumber())
    };
    ICImage* result = allocImage();
    if (!result) throw jsi::JSError(rt, "crop: allocation failed");
    int err = ic_crop(src, &region, result);
    if (err != IC_OK) { freeImage(result); checkError(rt, err, "crop"); }
    return wrapDecodedImage(rt, result);
}

jsi::Value ImageCoreHostObject::resize(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 2) throw jsi::JSError(rt, "resize requires 2 arguments");
    auto* src = extractNativeImage(rt, args[0], "resize");
    auto o = args[1].asObject(rt);
    uint32_t w = static_cast<uint32_t>(o.getProperty(rt, "width").asNumber());
    uint32_t h = static_cast<uint32_t>(o.getProperty(rt, "height").asNumber());
    ICResizeFilter f = IC_FILTER_LANCZOS;
    if (o.hasProperty(rt, "filter")) {
        auto fs = o.getProperty(rt, "filter").asString(rt).utf8(rt);
        if (fs == "bilinear") f = IC_FILTER_BILINEAR;
        else if (fs == "nearest") f = IC_FILTER_NEAREST;
    }
    ICImage* result = allocImage();
    if (!result) throw jsi::JSError(rt, "resize: allocation failed");
    int err = ic_resize(src, w, h, f, result);
    if (err != IC_OK) { freeImage(result); checkError(rt, err, "resize"); }
    return wrapDecodedImage(rt, result);
}

jsi::Value ImageCoreHostObject::rotate(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 2) throw jsi::JSError(rt, "rotate requires 2 arguments");
    auto* src = extractNativeImage(rt, args[0], "rotate");
    ICImage* result = allocImage();
    if (!result) throw jsi::JSError(rt, "rotate: allocation failed");
    int err = ic_rotate(src, static_cast<ICRotation>(static_cast<int>(args[1].asNumber())), result);
    if (err != IC_OK) { freeImage(result); checkError(rt, err, "rotate"); }
    return wrapDecodedImage(rt, result);
}

jsi::Value ImageCoreHostObject::flipHorizontal(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 1) throw jsi::JSError(rt, "flipHorizontal requires 1 argument");
    auto* src = extractNativeImage(rt, args[0], "flipHorizontal");
    ICImage* copy = cloneImage(src);
    if (!copy) throw jsi::JSError(rt, "flipHorizontal: allocation failed");
    int err = ic_flip_horizontal(copy);
    if (err != IC_OK) { freeImage(copy); checkError(rt, err, "flipHorizontal"); }
    return wrapDecodedImage(rt, copy);
}

jsi::Value ImageCoreHostObject::flipVertical(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
    if (count < 1) throw jsi::JSError(rt, "flipVertical requires 1 argument");
    auto* src = extractNativeImage(rt, args[0], "flipVertical");
    ICImage* copy = cloneImage(src);
    if (!copy) throw jsi::JSError(rt, "flipVertical: allocation failed");
    int err = ic_flip_vertical(copy);
    if (err != IC_OK) { freeImage(copy); checkError(rt, err, "flipVertical"); }
    return wrapDecodedImage(rt, copy);
}

/* ── HostObject interface ────────────────────────────────────────────── */

std::vector<jsi::PropNameID> ImageCoreHostObject::getPropertyNames(jsi::Runtime& rt) {
    std::vector<jsi::PropNameID> names;
    const char* methods[] = {
        "getImageInfo", "decode", "encode", "jpegLosslessRotate", "jpegLosslessCrop",
        "jpegStripExif", "stripExif", "convert", "readExif",
        "crop", "resize", "rotate", "flipHorizontal", "flipVertical"
    };
    names.reserve(sizeof(methods) / sizeof(methods[0]));
    for (auto m : methods) names.push_back(jsi::PropNameID::forAscii(rt, m));
    return names;
}

jsi::Value ImageCoreHostObject::get(jsi::Runtime& rt, const jsi::PropNameID& name) {
    auto n = name.utf8(rt);

    #define FN(jsName, fn, argc) \
        if (n == jsName) return jsi::Function::createFromHostFunction(rt, name, argc, \
            [](jsi::Runtime& r, const jsi::Value&, const jsi::Value* a, size_t c) -> jsi::Value { return fn(r, a, c); });

    FN("getImageInfo",       getImageInfo,       1)
    FN("decode",             decode,             1)
    FN("encode",             encode,             2)
    FN("jpegLosslessRotate", jpegLosslessRotate, 2)
    FN("jpegLosslessCrop",   jpegLosslessCrop,   2)
    FN("jpegStripExif",      jpegStripExif,      1)
    FN("stripExif",          stripExif,          1)
    FN("convert",            convert,            2)
    FN("readExif",           readExif,           1)
    FN("crop",               crop,               2)
    FN("resize",             resize,             2)
    FN("rotate",             rotate,             2)
    FN("flipHorizontal",     flipHorizontal,     1)
    FN("flipVertical",       flipVertical,       1)

    #undef FN
    return jsi::Value::undefined();
}

void install(jsi::Runtime& rt) {
    auto obj = jsi::Object::createFromHostObject(rt, std::make_shared<ImageCoreHostObject>());
    rt.global().setProperty(rt, "__ImageCoreProxy", std::move(obj));
}

} // namespace imagecore
