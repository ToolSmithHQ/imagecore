#pragma once

#include <jsi/jsi.h>
#include <string>

namespace imagecore {

using namespace facebook;

class ImageCoreHostObject : public jsi::HostObject {
public:
    ImageCoreHostObject() = default;

    jsi::Value get(jsi::Runtime& rt, const jsi::PropNameID& name) override;
    std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime& rt) override;

private:
    static std::pair<const uint8_t*, size_t>
    getArrayBufferData(jsi::Runtime& rt, const jsi::Value& val);

    static jsi::Value
    makeArrayBuffer(jsi::Runtime& rt, const uint8_t* data, size_t len);

    static jsi::Value
    makeImageInfo(jsi::Runtime& rt, uint32_t width, uint32_t height,
                  const char* format, bool hasExif, size_t fileSize);

    static jsi::Value getImageInfo(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value decode(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value encode(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value jpegLosslessRotate(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value jpegLosslessCrop(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value jpegStripExif(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value stripExif(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value convert(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value readExif(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value crop(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value resize(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value rotate(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value flipHorizontal(jsi::Runtime& rt, const jsi::Value* args, size_t count);
    static jsi::Value flipVertical(jsi::Runtime& rt, const jsi::Value* args, size_t count);
};

void install(jsi::Runtime& rt);

} // namespace imagecore
