# ImageCore

Native C/C++ image processing library for React Native (JSI) and Web (WASM). MIT-licensed.

## Features

- **Format conversion** — JPEG, PNG, WebP, BMP, TIFF, HEIC (AVIF on macOS/WASM only)
- **Lossless JPEG** — rotate 90/180/270, crop at MCU boundaries, binary EXIF strip (zero quality loss)
- **Pixel operations** — resize (Lanczos/bilinear/nearest), crop, rotate, flip
- **EXIF metadata** — read and strip without re-encoding (JPEG)
- **Platform HEIC** — iOS ImageIO + Android HeifWriter for HEIC encode/decode
- **Dual target** — same C++ core compiles to JSI (React Native) and WASM (Web)

## Usage

### React Native

```bash
npm install @toolsmith/imagecore-native @toolsmith/imagecore-types
```

```typescript
import { ImageCore } from '@toolsmith/imagecore-native';

// Convert JPEG to PNG
const inputBuffer = await readFileAsArrayBuffer('photo.jpeg');
const pngBuffer = ImageCore.convert(inputBuffer, { format: 'png', quality: 0.9 });

// Lossless JPEG rotate (zero quality loss)
const rotated = ImageCore.jpegLosslessRotate(inputBuffer, 90);

// Decode → resize → encode
const decoded = ImageCore.decode(inputBuffer);
const resized = ImageCore.resize(decoded, { width: 800, height: 600 });
const output = ImageCore.encode(resized, { format: 'webp', quality: 0.85 });
resized.free();
decoded.free();

// Strip EXIF metadata (binary removal for JPEG, no re-encoding)
const stripped = ImageCore.stripExif(inputBuffer);

// Read EXIF data
const exif = ImageCore.readExif(inputBuffer);
// { Make: 'Apple', Model: 'iPhone 15', DateTime: '2024:01:15 10:30:00', ... }
```

### Web (coming soon)

```bash
npm install @toolsmith/imagecore-web @toolsmith/imagecore-types
```

```typescript
import { createImageCore } from '@toolsmith/imagecore-web';

const core = await createImageCore(); // loads WASM module
const pngBuffer = core.convert(inputBuffer, { format: 'png', quality: 0.9 });
// Same API as @toolsmith/imagecore-native
```

## Packages

```
@toolsmith/imagecore-types     ← shared TypeScript types (published to npm)
         ↑
@toolsmith/imagecore-native    ← React Native JSI bindings (published to npm)
         ↑                       includes prebuilt C++ core (.a files)
         │
@toolsmith/imagecore-web       ← Web WASM bindings (published to npm)
         ↑                       includes prebuilt C++ core (.wasm)
         │
packages/core                  ← C++ source (NOT published to npm)
                                  compiled into native/web via build scripts
```

| npm Package | Install | Platform |
|---|---|---|
| `@toolsmith/imagecore-types` | `npm install @toolsmith/imagecore-types` | All |
| `@toolsmith/imagecore-native` | `npm install @toolsmith/imagecore-native` | React Native (iOS + Android) |
| `@toolsmith/imagecore-web` | `npm install @toolsmith/imagecore-web` | Web (browser) |

> `packages/core` is the C++ source code. It is not published to npm. It gets compiled into prebuilt `.a` files (iOS/Android) or `.wasm` (Web) that ship inside the native/web packages.

## API

All methods are synchronous (JSI). The web package requires an async `createImageCore()` init, then all methods are synchronous.

| Method | Description |
|---|---|
| `getImageInfo(data)` | Get dimensions, format, EXIF presence without full decode |
| `decode(data)` | Decode any format to RGBA pixels |
| `encode(image, opts)` | Encode pixels to any format |
| `convert(data, opts)` | Decode + encode in one call (format conversion) |
| `jpegLosslessRotate(data, 90\|180\|270)` | Lossless JPEG rotate (DCT block rearrangement) |
| `jpegLosslessCrop(data, region)` | Lossless JPEG crop at MCU boundaries |
| `jpegStripExif(data)` | Binary EXIF removal from JPEG (no re-encode) |
| `resize(image, opts)` | Resize decoded image (Lanczos/bilinear/nearest) |
| `crop(image, region)` | Crop decoded image |
| `rotate(image, 90\|180\|270)` | Rotate decoded image |
| `flipHorizontal(image)` | Flip decoded image horizontally |
| `flipVertical(image)` | Flip decoded image vertically |
| `readExif(data)` | Read EXIF metadata tags |
| `stripExif(data)` | Strip metadata from any format |

## Tests

### C++ Tests (63 tests)

```bash
# Build (requires CMake)
cd packages/core
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIC_BUILD_TESTS=ON
cmake --build build --config Release -j$(sysctl -n hw.logicalcpu)

# Run without sample images (38 tests)
./build/test_imagecore

# Run with sample images (63 tests)
./build/test_imagecore "$(pwd)/tests/samples/"
```

Test files:

| File | What it tests |
|---|---|
| `test_format_detection.cpp` | Magic byte detection, getImageInfo, null handling |
| `test_codec_roundtrips.cpp` | Encode/decode round-trips, cross-format conversion |
| `test_lossless_jpeg.cpp` | Lossless JPEG rotate, EXIF strip |
| `test_pixel_ops.cpp` | Crop, resize, rotate, flip, integration |
| `test_sample_images.cpp` | Real sample file decode, convert, operations |

### Rebuilding Prebuilt Libraries

```bash
# iOS (device + simulator)
bash scripts/build-ios.sh

# Android (arm64-v8a)
bash scripts/build-android.sh
```

## C++ Libraries Used

| Library | License | Purpose |
|---|---|---|
| [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo) | BSD | JPEG codec + lossless transforms |
| [libpng](https://github.com/pnggroup/libpng) | libpng (MIT-like) | PNG codec |
| [libwebp](https://chromium.googlesource.com/webm/libwebp) | BSD | WebP codec (lossy + lossless) |
| [libtiff](https://gitlab.com/libtiff/libtiff) | BSD-like | TIFF codec |
| [libavif](https://github.com/AOMediaCodec/libavif) | BSD | AVIF container (macOS/WASM only) |
| [libaom](https://aomedia.googlesource.com/aom) | BSD | AV1 codec for AVIF (macOS/WASM only) |
| [zlib](https://github.com/madler/zlib) | zlib | Compression (used by libpng, libtiff) |

HEIC encode/decode uses platform APIs (iOS ImageIO, Android HeifWriter) — no LGPL dependencies on mobile.

## License

MIT
