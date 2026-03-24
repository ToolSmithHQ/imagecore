# ImageCore

Native C/C++ image processing for React Native. Lossless JPEG ops, full format support, MIT-licensed.

## Features

- **Format conversion** — JPEG, PNG, WebP, BMP, TIFF, HEIC, AVIF
- **Lossless JPEG** — rotate 90/180/270, crop at MCU boundaries, strip EXIF (zero quality loss)
- **Pixel operations** — resize (Lanczos/bilinear/nearest), crop, rotate, flip
- **EXIF metadata** — read and strip without re-encoding
- **Platform HEIC** — iOS ImageIO + Android HeifWriter (no LGPL dependencies)
- **Synchronous JSI** — all operations run on the JS thread via C++ (no async bridge)

## Quick Start

```bash
npm install @toolsmith/imagecore-native @toolsmith/imagecore-files
```

```typescript
import { convertFile, resizeFile, getFileImageInfo } from '@toolsmith/imagecore-files';

// Convert HEIC to JPEG
const jpegUri = await convertFile('file:///path/to/photo.heic', 'jpeg', 0.85);

// Resize to thumbnail
const thumbUri = await resizeFile(jpegUri, 200, 200, 'jpeg');

// Get image dimensions
const info = await getFileImageInfo(jpegUri);
console.log(info.width, info.height, info.format);
```

That's it. File in, file out. No ArrayBuffers, no manual memory management.

## File API (`@toolsmith/imagecore-files`)

The recommended way to use imagecore in React Native. All functions take file URIs and return file URIs.

### Convert format

```typescript
import { convertFile } from '@toolsmith/imagecore-files';

const pngUri = await convertFile(sourceUri, 'png');           // default quality: 0.85
const jpegUri = await convertFile(sourceUri, 'jpeg', 0.9);  // custom quality
const webpUri = await convertFile(sourceUri, 'webp', 0.85);
const heicUri = await convertFile(sourceUri, 'heic', 0.8);
```

### Resize

```typescript
import { resizeFile } from '@toolsmith/imagecore-files';

const resizedUri = await resizeFile(sourceUri, 800, 600, 'jpeg');  // default quality: 0.95
const thumbUri = await resizeFile(sourceUri, 200, 200, 'png', 0.8);  // custom quality
```

### Crop

```typescript
import { cropFile } from '@toolsmith/imagecore-files';

// cropFile(uri, x, y, width, height, outputFormat, quality?)
const croppedUri = await cropFile(sourceUri, 100, 50, 400, 300, 'jpeg');  // default quality: 0.95
```

### Rotate

```typescript
import { rotateFile } from '@toolsmith/imagecore-files';

const rotatedUri = await rotateFile(sourceUri, 90, 'jpeg');   // 90, 180, or 270. default quality: 0.95
```

### Flip

```typescript
import { flipFile } from '@toolsmith/imagecore-files';

// flipFile(uri, horizontal, vertical, outputFormat, quality?). default quality: 0.95
const mirroredUri = await flipFile(sourceUri, true, false, 'png');   // horizontal flip
const flippedUri = await flipFile(sourceUri, false, true, 'png');    // vertical flip
const bothUri = await flipFile(sourceUri, true, true, 'png');        // both
```

### Compress

```typescript
import { compressFile } from '@toolsmith/imagecore-files';

// Re-encode at lower quality
const compressedUri = await compressFile(sourceUri, 0.5, 'jpeg');
```

### Lossless JPEG rotate

Rotates JPEG without decoding pixels. Zero quality loss — rearranges DCT blocks directly.

```typescript
import { losslessJpegRotate } from '@toolsmith/imagecore-files';

const rotatedUri = await losslessJpegRotate(sourceUri, 90);  // 90, 180, or 270
```

### Image info

```typescript
import { getFileImageInfo } from '@toolsmith/imagecore-files';

const info = await getFileImageInfo(sourceUri);
// { width: 4032, height: 3024, format: 'jpeg', hasExif: true, fileSize: 2048576 }
```

### EXIF metadata

```typescript
import { readFileExif, stripFileExif } from '@toolsmith/imagecore-files';

// Read EXIF tags
const exif = await readFileExif(sourceUri);
// { Make: 'Apple', Model: 'iPhone 15', DateTime: '2024:01:15 10:30:00', ... }

// Strip all EXIF metadata (binary removal for JPEG — no re-encoding)
const strippedUri = await stripFileExif(sourceUri);
```

### Utility functions

```typescript
import { readFileAsArrayBuffer, writeArrayBufferToFile } from '@toolsmith/imagecore-files';

// Read file to ArrayBuffer (for custom workflows)
const buffer = await readFileAsArrayBuffer(sourceUri);

// Write ArrayBuffer to cache file
const outputUri = writeArrayBufferToFile(buffer, 'png');
```

## Low-Level API (`@toolsmith/imagecore-native`)

For working with in-memory buffers, chaining multiple operations without intermediate files, or custom workflows.

```bash
npm install @toolsmith/imagecore-native
```

```typescript
import { ImageCore } from '@toolsmith/imagecore-native';

// All methods are synchronous (JSI) and operate on ArrayBuffers.
// DecodedImage objects must be freed when done.

// Convert format (one-liner)
const pngBuffer = ImageCore.convert(inputBuffer, { format: 'png', quality: 0.9 });

// Lossless JPEG rotate (zero quality loss)
const rotated = ImageCore.jpegLosslessRotate(jpegBuffer, 90);

// Decode → resize → encode (manual pipeline)
const decoded = ImageCore.decode(inputBuffer);
const resized = ImageCore.resize(decoded, { width: 800, height: 600, filter: 'lanczos' });
const output = ImageCore.encode(resized, { format: 'webp', quality: 0.85 });
resized.free();  // release native memory
decoded.free();

// EXIF
const exif = ImageCore.readExif(jpegBuffer);
const stripped = ImageCore.stripExif(jpegBuffer);
```

### Full API reference

All methods are synchronous. `data` is `ArrayBuffer`, `image` is `DecodedImage`.

| Method | Description |
|---|---|
| `getImageInfo(data)` | Get dimensions, format, EXIF presence without full decode |
| `decode(data)` | Decode any format to RGBA pixels (returns `DecodedImage`) |
| `encode(image, opts)` | Encode pixels to any format (returns `ArrayBuffer`) |
| `convert(data, opts)` | Decode + encode in one call |
| `jpegLosslessRotate(data, 90\|180\|270)` | Lossless JPEG rotate via DCT block rearrangement |
| `jpegLosslessCrop(data, region)` | Lossless JPEG crop at MCU boundaries |
| `jpegStripExif(data)` | Binary EXIF removal from JPEG (no re-encode) |
| `resize(image, opts)` | Resize with Lanczos, bilinear, or nearest filter |
| `crop(image, region)` | Crop to `{ x, y, width, height }` |
| `rotate(image, 90\|180\|270)` | Rotate decoded image |
| `flipHorizontal(image)` | Mirror horizontally |
| `flipVertical(image)` | Flip vertically |
| `readExif(data)` | Read EXIF metadata tags |
| `stripExif(data)` | Strip metadata from any format |

### Memory management

`decode()`, `resize()`, `crop()`, `rotate()`, `flipHorizontal()`, and `flipVertical()` return `DecodedImage` objects that hold native memory. Always call `.free()` when done:

```typescript
const decoded = ImageCore.decode(buffer);
try {
  const resized = ImageCore.resize(decoded, { width: 400, height: 300 });
  try {
    const output = ImageCore.encode(resized, { format: 'jpeg', quality: 0.85 });
    // use output...
  } finally {
    resized.free();
  }
} finally {
  decoded.free();
}
```

The file API (`@toolsmith/imagecore-files`) handles this automatically.

## Packages

```
@toolsmith/imagecore-types     ← shared TypeScript types
         |
    +---------+
    |         |
imagecore-native  imagecore-web    ← same API, different engines (JSI vs WASM)
    |
imagecore-files                    ← high-level file URI convenience layer (React Native)
```

| Package | Install | Platform |
|---|---|---|
| `@toolsmith/imagecore-files` | `npm install @toolsmith/imagecore-files` | React Native (iOS + Android) |
| `@toolsmith/imagecore-native` | `npm install @toolsmith/imagecore-native` | React Native (iOS + Android) |
| `@toolsmith/imagecore-types` | `npm install @toolsmith/imagecore-types` | All |
| `@toolsmith/imagecore-web` | `npm install @toolsmith/imagecore-web` | Web (not yet available) |

> `packages/core` contains the C++ source. It is not published to npm — it gets compiled into prebuilt `.a` files (iOS/Android) that ship inside `@toolsmith/imagecore-native`.

## Supported Formats

| Format | iOS | Android | macOS (tests) | WASM |
|---|---|---|---|---|
| JPEG | decode + encode | decode + encode | decode + encode | decode + encode |
| PNG | decode + encode | decode + encode | decode + encode | decode + encode |
| WebP | decode + encode | decode + encode | decode + encode | decode + encode |
| BMP | decode + encode | decode + encode | decode + encode | decode + encode |
| TIFF | decode + encode | decode + encode | decode + encode | decode + encode |
| HEIC | decode + encode | decode + encode | — | — |
| AVIF | — | — | decode + encode | decode + encode |

- **HEIC** uses platform APIs: iOS `ImageIO.framework`, Android `HeifWriter` (API 30+). No LGPL dependencies.
- **AVIF** uses libaom. Not cross-compiled for mobile yet — works on macOS and WASM only.

## Web

The web package (`@toolsmith/imagecore-web`) is not yet available. It will provide the same API backed by WASM:

```typescript
import { createImageCore } from '@toolsmith/imagecore-web';

const core = await createImageCore(); // loads WASM module
const pngBuffer = core.convert(inputBuffer, { format: 'png', quality: 0.9 });
// Same API as @toolsmith/imagecore-native
```

> **Why no `imagecore-files` for web?** On React Native, reading a file URI into an ArrayBuffer requires `expo-file-system` with base64 encoding/decoding — that's the boilerplate `imagecore-files` eliminates. On web, the browser has this built in:

```typescript
// Web: reading a file is one line
const buffer = await file.arrayBuffer();  // File from <input type="file">

// Web: displaying the result is one line
const blob = new Blob([outputBuffer], { type: 'image/png' });
const url = URL.createObjectURL(blob);    // use in <img src={url}>

// Web: downloading the result
const a = document.createElement('a');
a.href = url;
a.download = 'output.png';
a.click();
```

No bridge package needed — the browser's File API handles it natively.

## Tests

### C++ Tests (63 tests)

```bash
make test               # 38 tests (no sample images)
make test-samples       # 63 tests (with sample images)
```

Or manually:

```bash
cd packages/core
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIC_BUILD_TESTS=ON
cmake --build build --config Release -j$(sysctl -n hw.logicalcpu)

./build/test_imagecore                          # 38 tests
./build/test_imagecore "$(pwd)/tests/samples/"  # 63 tests
```

| Test file | What it tests |
|---|---|
| `test_format_detection.cpp` | Magic byte detection, getImageInfo, null handling |
| `test_codec_roundtrips.cpp` | Encode/decode round-trips, cross-format conversion |
| `test_lossless_jpeg.cpp` | Lossless JPEG rotate, EXIF strip |
| `test_pixel_ops.cpp` | Crop, resize, rotate, flip, integration |
| `test_sample_images.cpp` | Real sample file decode, convert, operations |

### Rebuilding Prebuilt Libraries

```bash
make build-ios       # iOS device + simulator (arm64)
make build-android   # Android arm64-v8a
```

## C++ Libraries

| Library | License | Purpose |
|---|---|---|
| [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo) | BSD | JPEG codec + lossless transforms |
| [libpng](https://github.com/pnggroup/libpng) | libpng (MIT-like) | PNG codec |
| [libwebp](https://chromium.googlesource.com/webm/libwebp) | BSD | WebP codec (lossy + lossless) |
| [libtiff](https://gitlab.com/libtiff/libtiff) | BSD-like | TIFF codec |
| [libavif](https://github.com/AOMediaCodec/libavif) | BSD | AVIF container (macOS/WASM only) |
| [libaom](https://aomedia.googlesource.com/aom) | BSD | AV1 codec for AVIF (macOS/WASM only) |
| [zlib](https://github.com/madler/zlib) | zlib | Compression (used by libpng, libtiff) |

HEIC uses platform APIs (iOS ImageIO, Android HeifWriter) — no LGPL dependencies on mobile.

## License

MIT
