# @toolsmithhq/imagecore-types

Shared TypeScript types for the [ImageCore](https://github.com/ToolSmithHQ/imagecore) image processing library.

You typically don't need to install this directly — it's already a dependency of `@toolsmithhq/imagecore-native` and `@toolsmithhq/imagecore-files`.

## Types

```typescript
import type {
  ImageCore,        // Main API interface
  ImageInfo,        // { width, height, format, hasExif, fileSize }
  ImageFormat,      // 'jpeg' | 'png' | 'webp' | 'heic' | 'avif' | 'tiff' | 'bmp' | 'gif'
  DecodedImage,     // Opaque handle to decoded RGBA pixels (has .free())
  EncodeOptions,    // { format, quality?, lossless?, stripExif? }
  ResizeOptions,    // { width, height, filter? }
  CropRegion,       // { x, y, width, height }
  Rotation,         // 90 | 180 | 270
  ExifData,         // { [key: string]: string | number | boolean | undefined }
  ResizeFilter,     // 'lanczos' | 'bilinear' | 'nearest'
} from '@toolsmithhq/imagecore-types';
```

## License

MIT
