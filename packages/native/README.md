# @toolsmithhq/imagecore-native

Native C/C++ image processing for React Native via JSI. Synchronous, zero-copy, MIT-licensed.

Part of [ImageCore](https://github.com/ToolSmithHQ/imagecore). For the easier file-based API, see [`@toolsmithhq/imagecore-files`](https://www.npmjs.com/package/@toolsmithhq/imagecore-files).

## Quick Start (recommended)

Most apps should use [`@toolsmithhq/imagecore-files`](https://www.npmjs.com/package/@toolsmithhq/imagecore-files) — a high-level wrapper that handles file I/O and memory management for you:

```bash
npm install @toolsmithhq/imagecore-native @toolsmithhq/imagecore-files
```

```typescript
import {
  convertFile, resizeFile, cropFile, rotateFile, flipFile,
  compressFile, losslessJpegRotate, getFileImageInfo,
  readFileExif, stripFileExif,
} from '@toolsmithhq/imagecore-files';

// Convert HEIC to JPEG
const jpegUri = await convertFile(sourceUri, 'jpeg', 0.85);         // default quality: 0.85

// Resize
const thumbUri = await resizeFile(sourceUri, 200, 200, 'jpeg');     // default quality: 0.95

// Crop
const croppedUri = await cropFile(sourceUri, 100, 50, 400, 300, 'jpeg');

// Rotate
const rotatedUri = await rotateFile(sourceUri, 90, 'jpeg');         // 90, 180, or 270

// Flip
const mirroredUri = await flipFile(sourceUri, true, false, 'png');  // horizontal flip

// Compress
const compressedUri = await compressFile(sourceUri, 0.5, 'jpeg');

// Lossless JPEG rotate (zero quality loss — rearranges DCT blocks directly)
const losslessUri = await losslessJpegRotate(sourceUri, 90);

// Image info
const info = await getFileImageInfo(sourceUri);
// { width: 4032, height: 3024, format: 'jpeg', hasExif: true, fileSize: 2048576 }

// EXIF metadata
const exif = await readFileExif(sourceUri);
// { Make: 'Apple', Model: 'iPhone 15', DateTime: '2024:01:15 10:30:00', ... }

// Strip EXIF (binary removal for JPEG — no re-encoding)
const strippedUri = await stripFileExif(sourceUri);
```

File in, file out. No ArrayBuffers, no manual memory management.

## Install (low-level only)

```bash
npm install @toolsmithhq/imagecore-native
```

## Features

- **Format conversion** — JPEG, PNG, WebP, BMP, TIFF, HEIC, AVIF
- **Lossless JPEG** — rotate, crop, EXIF strip (zero quality loss)
- **Pixel operations** — resize (Lanczos/bilinear/nearest), crop, rotate, flip
- **EXIF metadata** — read and strip
- **Synchronous JSI** — all operations run on the JS thread via C++ (no async bridge)
- **Platform HEIC** — iOS ImageIO + Android HeifWriter (no LGPL dependencies)

## Usage

All methods are synchronous and operate on ArrayBuffers. `DecodedImage` objects must be freed when done.

```typescript
import { ImageCore } from '@toolsmithhq/imagecore-native';

// Convert format
const pngBuffer = ImageCore.convert(inputBuffer, { format: 'png', quality: 0.9 });

// Lossless JPEG rotate (zero quality loss)
const rotated = ImageCore.jpegLosslessRotate(jpegBuffer, 90);

// Decode → resize → encode
const decoded = ImageCore.decode(inputBuffer);
const resized = ImageCore.resize(decoded, { width: 800, height: 600, filter: 'lanczos' });
const output = ImageCore.encode(resized, { format: 'webp', quality: 0.85 });
resized.free();
decoded.free();

// EXIF
const exif = ImageCore.readExif(jpegBuffer);
const stripped = ImageCore.stripExif(jpegBuffer);
```

## API

| Method | Description |
|---|---|
| `getImageInfo(data)` | Get dimensions, format, EXIF presence without full decode |
| `decode(data)` | Decode any format to RGBA pixels |
| `encode(image, opts)` | Encode pixels to any format |
| `convert(data, opts)` | Decode + encode in one call |
| `jpegLosslessRotate(data, 90\|180\|270)` | Lossless JPEG rotate via DCT block rearrangement |
| `jpegLosslessCrop(data, region)` | Lossless JPEG crop at MCU boundaries |
| `jpegStripExif(data)` | Binary EXIF removal from JPEG |
| `resize(image, opts)` | Resize with Lanczos, bilinear, or nearest filter |
| `crop(image, region)` | Crop to `{ x, y, width, height }` |
| `rotate(image, 90\|180\|270)` | Rotate decoded image |
| `flipHorizontal(image)` | Mirror horizontally |
| `flipVertical(image)` | Flip vertically |
| `readExif(data)` | Read EXIF metadata tags |
| `stripExif(data)` | Strip metadata from any format |

## Memory Management

`decode()`, `resize()`, `crop()`, `rotate()`, `flipHorizontal()`, and `flipVertical()` return `DecodedImage` objects that hold native memory. Always call `.free()` when done.

If you don't want to manage memory manually, use [`@toolsmithhq/imagecore-files`](https://www.npmjs.com/package/@toolsmithhq/imagecore-files) instead.

## License

MIT
