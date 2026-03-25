/**
 * @toolsmithhq/imagecore-web
 *
 * WASM-based image processing for web/React apps.
 * Same API as @toolsmithhq/imagecore-native.
 *
 * Usage:
 *   import { createImageCore } from '@toolsmithhq/imagecore-web';
 *   const core = await createImageCore();
 *   const rotated = core.jpegLosslessRotate(buffer, 90);
 */

import type {
  ImageCore as IImageCore,
  ImageInfo,
  EncodeOptions,
  DecodedImage,
  Rotation,
  CropRegion,
  ResizeOptions,
  ExifData,
} from '@toolsmithhq/imagecore-types';

export type {
  ImageInfo,
  EncodeOptions,
  DecodedImage,
  Rotation,
  CropRegion,
  ResizeOptions,
  ExifData,
};

export { ImageFormat } from '@toolsmithhq/imagecore-types';

/* ── WASM module interface ───────────────────────────────────────────── */

interface WasmModule {
  _ic_detect_format(dataPtr: number, len: number): number;
  _ic_get_image_info(dataPtr: number, len: number, infoPtr: number): number;
  _ic_decode(dataPtr: number, len: number, imgPtr: number): number;
  _ic_encode(imgPtr: number, fmt: number, optsPtr: number,
             outDataPtr: number, outLenPtr: number): number;
  _ic_convert(dataPtr: number, len: number, fmt: number, optsPtr: number,
              outDataPtr: number, outLenPtr: number): number;
  _ic_jpeg_lossless_rotate(dataPtr: number, len: number, rotation: number,
                           outDataPtr: number, outLenPtr: number): number;
  _ic_jpeg_lossless_crop(dataPtr: number, len: number, regionPtr: number,
                         outDataPtr: number, outLenPtr: number): number;
  _ic_jpeg_strip_exif(dataPtr: number, len: number,
                      outDataPtr: number, outLenPtr: number): number;
  _ic_strip_exif(dataPtr: number, len: number,
                 outDataPtr: number, outLenPtr: number): number;
  _ic_free(ptr: number): void;
  _ic_image_free(ptr: number): void;
  _ic_version(): number;
  _ic_read_exif_internal(dataPtr: number, len: number, resultPtr: number): number;
  _malloc(size: number): number;
  _free(ptr: number): void;
  HEAPU8: Uint8Array;
  HEAPU32: Uint32Array;
  HEAPF32: Float32Array;
  UTF8ToString(ptr: number): string;
  getValue(ptr: number, type: string): number;
  setValue(ptr: number, value: number, type: string): void;
}

/* ── Format enum mapping ─────────────────────────────────────────────── */

const FORMAT_MAP: Record<string, number> = {
  unknown: 0, jpeg: 1, png: 2, webp: 3, tiff: 4,
  bmp: 5, avif: 6, heic: 7, gif: 8,
};

const FORMAT_REVERSE: Record<number, string> = {
  0: 'unknown', 1: 'jpeg', 2: 'png', 3: 'webp', 4: 'tiff',
  5: 'bmp', 6: 'avif', 7: 'heic', 8: 'gif',
};

/* ── Helper functions ────────────────────────────────────────────────── */

function copyToWasm(wasm: WasmModule, data: ArrayBuffer): [number, number] {
  const bytes = new Uint8Array(data);
  const ptr = wasm._malloc(bytes.length);
  if (!ptr) throw new Error('WASM malloc failed');
  wasm.HEAPU8.set(bytes, ptr);
  return [ptr, bytes.length];
}

function copyFromWasm(wasm: WasmModule, outDataPtr: number, outLenPtr: number): ArrayBuffer {
  const dataPtr = wasm.getValue(outDataPtr, 'i32');
  const dataLen = wasm.getValue(outLenPtr, 'i32');
  const result = new ArrayBuffer(dataLen);
  new Uint8Array(result).set(wasm.HEAPU8.subarray(dataPtr, dataPtr + dataLen));
  wasm._ic_free(dataPtr);
  return result;
}

function checkError(err: number, operation: string): void {
  if (err === 0) return;
  const messages: Record<number, string> = {
    [-1]: 'Invalid input',
    [-2]: 'Unsupported format',
    [-3]: 'Decode failed',
    [-4]: 'Encode failed',
    [-5]: 'Memory allocation failed',
    [-6]: 'Invalid rotation',
    [-7]: 'Invalid crop region',
    [-8]: 'EXIF not found',
  };
  throw new Error(`ImageCore.${operation}: ${messages[err] || `Unknown error ${err}`}`);
}

/* ── ICImageInfo struct layout (C) ───────────────────────────────────── */
// uint32_t width, height; ICFormat format; int has_exif; size_t file_size;
// = 4 + 4 + 4 + 4 + 4 = 20 bytes (with padding)
const SIZEOF_IMAGE_INFO = 24; // aligned

/* ── ICImage struct layout (C) ───────────────────────────────────────── */
// uint8_t* pixels; uint32_t width, height, stride;
// On WASM (32-bit): 4 + 4 + 4 + 4 = 16 bytes
const SIZEOF_IMAGE = 16;

/* ── ICEncodeOpts struct layout (C) ──────────────────────────────────── */
// float quality; int lossless; int strip_exif;
const SIZEOF_ENCODE_OPTS = 12;

/* ── ICCropRegion struct layout (C) ──────────────────────────────────── */
// uint32_t x, y, width, height;
const SIZEOF_CROP_REGION = 16;

/* ── Main factory ────────────────────────────────────────────────────── */

/**
 * Create an ImageCore instance by loading and initializing the WASM module.
 * Call once at app startup, then reuse the returned object.
 */
export async function createImageCore(): Promise<IImageCore> {
  // Dynamic import of the Emscripten-generated JS loader
  // eslint-disable-next-line @typescript-eslint/no-require-imports
  const factory = require('./imagecore.js');
  const wasm: WasmModule = await factory();

  const core: IImageCore = {
    getImageInfo(data: ArrayBuffer): ImageInfo {
      const [dataPtr, len] = copyToWasm(wasm, data);
      const infoPtr = wasm._malloc(SIZEOF_IMAGE_INFO);

      try {
        const err = wasm._ic_get_image_info(dataPtr, len, infoPtr);
        checkError(err, 'getImageInfo');

        return {
          width: wasm.getValue(infoPtr, 'i32'),
          height: wasm.getValue(infoPtr + 4, 'i32'),
          format: (FORMAT_REVERSE[wasm.getValue(infoPtr + 8, 'i32')] || 'unknown') as ImageInfo['format'],
          hasExif: wasm.getValue(infoPtr + 12, 'i32') !== 0,
          fileSize: wasm.getValue(infoPtr + 16, 'i32'),
        };
      } finally {
        wasm._free(dataPtr);
        wasm._free(infoPtr);
      }
    },

    decode(data: ArrayBuffer): DecodedImage {
      const [dataPtr, len] = copyToWasm(wasm, data);
      const imgPtr = wasm._malloc(SIZEOF_IMAGE);

      try {
        const err = wasm._ic_decode(dataPtr, len, imgPtr);
        checkError(err, 'decode');

        const width = wasm.getValue(imgPtr + 4, 'i32');  // after pixels ptr
        const height = wasm.getValue(imgPtr + 8, 'i32');

        // Keep imgPtr alive — it holds the pixel data
        const savedImgPtr = imgPtr;
        let freed = false;

        return {
          width,
          height,
          free() {
            if (!freed) {
              wasm._ic_image_free(savedImgPtr);
              wasm._free(savedImgPtr);
              freed = true;
            }
          },
          // Internal: store the WASM pointer for encode()
          _wasmPtr: savedImgPtr,
        } as DecodedImage & { _wasmPtr: number };
      } finally {
        wasm._free(dataPtr);
        // Don't free imgPtr — it's owned by DecodedImage now
      }
    },

    encode(image: DecodedImage, options: EncodeOptions): ArrayBuffer {
      const imgPtr = (image as DecodedImage & { _wasmPtr?: number })._wasmPtr;
      if (imgPtr === undefined) {
        throw new Error('encode: invalid DecodedImage');
      }

      const fmt = FORMAT_MAP[options.format];
      if (fmt === undefined) {
        throw new Error(`encode: unknown format '${options.format}'`);
      }

      const optsPtr = wasm._malloc(SIZEOF_ENCODE_OPTS);
      const outDataPtr = wasm._malloc(4);
      const outLenPtr = wasm._malloc(4);

      try {
        wasm.HEAPF32[optsPtr >> 2] = options.quality ?? 0.85;
        wasm.setValue(optsPtr + 4, options.lossless ? 1 : 0, 'i32');
        wasm.setValue(optsPtr + 8, options.stripExif ? 1 : 0, 'i32');

        const err = wasm._ic_encode(imgPtr, fmt, optsPtr, outDataPtr, outLenPtr);
        checkError(err, 'encode');

        return copyFromWasm(wasm, outDataPtr, outLenPtr);
      } finally {
        wasm._free(optsPtr);
        wasm._free(outDataPtr);
        wasm._free(outLenPtr);
      }
    },

    jpegLosslessRotate(data: ArrayBuffer, rotation: Rotation): ArrayBuffer {
      const [dataPtr, len] = copyToWasm(wasm, data);
      const outDataPtr = wasm._malloc(4);
      const outLenPtr = wasm._malloc(4);

      try {
        const err = wasm._ic_jpeg_lossless_rotate(dataPtr, len, rotation,
                                                    outDataPtr, outLenPtr);
        checkError(err, 'jpegLosslessRotate');
        return copyFromWasm(wasm, outDataPtr, outLenPtr);
      } finally {
        wasm._free(dataPtr);
        wasm._free(outDataPtr);
        wasm._free(outLenPtr);
      }
    },

    jpegLosslessCrop(data: ArrayBuffer, region: CropRegion): ArrayBuffer {
      const [dataPtr, len] = copyToWasm(wasm, data);
      const regionPtr = wasm._malloc(SIZEOF_CROP_REGION);
      const outDataPtr = wasm._malloc(4);
      const outLenPtr = wasm._malloc(4);

      try {
        wasm.setValue(regionPtr, region.x, 'i32');
        wasm.setValue(regionPtr + 4, region.y, 'i32');
        wasm.setValue(regionPtr + 8, region.width, 'i32');
        wasm.setValue(regionPtr + 12, region.height, 'i32');

        const err = wasm._ic_jpeg_lossless_crop(dataPtr, len, regionPtr,
                                                  outDataPtr, outLenPtr);
        checkError(err, 'jpegLosslessCrop');
        return copyFromWasm(wasm, outDataPtr, outLenPtr);
      } finally {
        wasm._free(dataPtr);
        wasm._free(regionPtr);
        wasm._free(outDataPtr);
        wasm._free(outLenPtr);
      }
    },

    jpegStripExif(data: ArrayBuffer): ArrayBuffer {
      const [dataPtr, len] = copyToWasm(wasm, data);
      const outDataPtr = wasm._malloc(4);
      const outLenPtr = wasm._malloc(4);

      try {
        const err = wasm._ic_jpeg_strip_exif(dataPtr, len, outDataPtr, outLenPtr);
        checkError(err, 'jpegStripExif');
        return copyFromWasm(wasm, outDataPtr, outLenPtr);
      } finally {
        wasm._free(dataPtr);
        wasm._free(outDataPtr);
        wasm._free(outLenPtr);
      }
    },

    resize(_image: DecodedImage, _options: ResizeOptions): DecodedImage {
      throw new Error('resize: not yet implemented for WASM');
    },

    crop(_image: DecodedImage, _region: CropRegion): DecodedImage {
      throw new Error('crop: not yet implemented (Phase 2)');
    },

    rotate(_image: DecodedImage, _rotation: Rotation): DecodedImage {
      throw new Error('rotate: not yet implemented (Phase 2)');
    },

    readExif(data: ArrayBuffer): ExifData {
      const [dataPtr, len] = copyToWasm(wasm, data);
      // ICExifResult: 64 entries * (2 + 32 + 128) + 4 = ~10372 bytes
      const SIZEOF_EXIF_RESULT = 10372;
      const resultPtr = wasm._malloc(SIZEOF_EXIF_RESULT);

      try {
        const err = wasm._ic_read_exif_internal(dataPtr, len, resultPtr);
        if (err === -8) return {}; // EXIF not found
        checkError(err, 'readExif');

        const count = wasm.getValue(resultPtr + 64 * 162, 'i32'); // offset to count field
        const result: ExifData = {};

        for (let i = 0; i < count; i++) {
          const entryBase = resultPtr + i * 162; // 2 + 32 + 128 = 162 per entry
          const namePtr = entryBase + 2;
          const valuePtr = entryBase + 34;
          const name = wasm.UTF8ToString(namePtr);
          const value = wasm.UTF8ToString(valuePtr);
          result[name] = value;
        }

        return result;
      } finally {
        wasm._free(dataPtr);
        wasm._free(resultPtr);
      }
    },

    stripExif(data: ArrayBuffer): ArrayBuffer {
      const [dataPtr, len] = copyToWasm(wasm, data);
      const outDataPtr = wasm._malloc(4);
      const outLenPtr = wasm._malloc(4);

      try {
        const err = wasm._ic_strip_exif(dataPtr, len, outDataPtr, outLenPtr);
        checkError(err, 'stripExif');
        return copyFromWasm(wasm, outDataPtr, outLenPtr);
      } finally {
        wasm._free(dataPtr);
        wasm._free(outDataPtr);
        wasm._free(outLenPtr);
      }
    },

    convert(data: ArrayBuffer, options: EncodeOptions): ArrayBuffer {
      const [dataPtr, len] = copyToWasm(wasm, data);
      const fmt = FORMAT_MAP[options.format];
      if (fmt === undefined) {
        wasm._free(dataPtr);
        throw new Error(`convert: unknown format '${options.format}'`);
      }

      const optsPtr = wasm._malloc(SIZEOF_ENCODE_OPTS);
      const outDataPtr = wasm._malloc(4);
      const outLenPtr = wasm._malloc(4);

      try {
        wasm.HEAPF32[optsPtr >> 2] = options.quality ?? 0.85;
        wasm.setValue(optsPtr + 4, options.lossless ? 1 : 0, 'i32');
        wasm.setValue(optsPtr + 8, options.stripExif ? 1 : 0, 'i32');

        const err = wasm._ic_convert(dataPtr, len, fmt, optsPtr,
                                       outDataPtr, outLenPtr);
        checkError(err, 'convert');
        return copyFromWasm(wasm, outDataPtr, outLenPtr);
      } finally {
        wasm._free(dataPtr);
        wasm._free(optsPtr);
        wasm._free(outDataPtr);
        wasm._free(outLenPtr);
      }
    },
  };

  return core;
}
