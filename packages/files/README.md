# @toolsmithhq/imagecore-files

High-level file-based image processing for React Native. File URI in, file URI out.

Part of [ImageCore](https://github.com/ToolSmithHQ/imagecore). Built on top of [`@toolsmithhq/imagecore-native`](https://www.npmjs.com/package/@toolsmithhq/imagecore-native).

## Install

```bash
npm install @toolsmithhq/imagecore-native @toolsmithhq/imagecore-files
```

## Quick Start

```typescript
import { convertFile, resizeFile, getFileImageInfo } from '@toolsmithhq/imagecore-files';

const jpegUri = await convertFile('file:///path/to/photo.heic', 'jpeg', 0.85);
const thumbUri = await resizeFile(jpegUri, 200, 200, 'jpeg');
const info = await getFileImageInfo(jpegUri);
```

No ArrayBuffers, no manual memory management.

## API

### Convert format

```typescript
import { convertFile } from '@toolsmithhq/imagecore-files';

const pngUri = await convertFile(sourceUri, 'png');           // default quality: 0.85
const jpegUri = await convertFile(sourceUri, 'jpeg', 0.9);  // custom quality
```

### Resize

```typescript
import { resizeFile } from '@toolsmithhq/imagecore-files';

const resizedUri = await resizeFile(sourceUri, 800, 600, 'jpeg');  // default quality: 0.95
```

### Crop

```typescript
import { cropFile } from '@toolsmithhq/imagecore-files';

// cropFile(uri, x, y, width, height, outputFormat, quality?)
const croppedUri = await cropFile(sourceUri, 100, 50, 400, 300, 'jpeg');  // default quality: 0.95
```

### Rotate

```typescript
import { rotateFile } from '@toolsmithhq/imagecore-files';

const rotatedUri = await rotateFile(sourceUri, 90, 'jpeg');  // 90, 180, or 270. default quality: 0.95
```

### Flip

```typescript
import { flipFile } from '@toolsmithhq/imagecore-files';

// flipFile(uri, horizontal, vertical, outputFormat, quality?). default quality: 0.95
const mirroredUri = await flipFile(sourceUri, true, false, 'png');  // horizontal flip
const flippedUri = await flipFile(sourceUri, false, true, 'png');   // vertical flip
```

### Compress

```typescript
import { compressFile } from '@toolsmithhq/imagecore-files';

const compressedUri = await compressFile(sourceUri, 0.5, 'jpeg');
```

### Lossless JPEG rotate

Zero quality loss — rearranges DCT blocks directly without decoding pixels.

```typescript
import { losslessJpegRotate } from '@toolsmithhq/imagecore-files';

const rotatedUri = await losslessJpegRotate(sourceUri, 90);
```

### Image info

```typescript
import { getFileImageInfo } from '@toolsmithhq/imagecore-files';

const info = await getFileImageInfo(sourceUri);
// { width: 4032, height: 3024, format: 'jpeg', hasExif: true, fileSize: 2048576 }
```

### EXIF metadata

```typescript
import { readFileExif, stripFileExif } from '@toolsmithhq/imagecore-files';

const exif = await readFileExif(sourceUri);
// { Make: 'Apple', Model: 'iPhone 15', DateTime: '2024:01:15 10:30:00', ... }

const strippedUri = await stripFileExif(sourceUri);
```

### Utility functions

```typescript
import { readFileAsArrayBuffer, writeArrayBufferToFile } from '@toolsmithhq/imagecore-files';

const buffer = await readFileAsArrayBuffer(sourceUri);
const outputUri = writeArrayBufferToFile(buffer, 'png');
```

## Supported Formats

JPEG, PNG, WebP, BMP, TIFF, HEIC (iOS + Android), AVIF (macOS/WASM only)

## License

MIT
