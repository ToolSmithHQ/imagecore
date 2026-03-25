/**
 * @toolsmithhq/imagecore-files
 *
 * High-level file-based API for imagecore on React Native.
 * Wraps imagecore's ArrayBuffer API with file URI convenience functions.
 *
 * Usage:
 *   import { convertFile, resizeFile } from '@toolsmithhq/imagecore-files';
 *   const jpegUri = await convertFile('file:///photo.heic', 'jpeg', 0.85);
 */

import { File, Paths, Directory } from 'expo-file-system/next';
import { ImageCore, ImageFormat } from '@toolsmithhq/imagecore-native';
import type { Rotation, ImageInfo } from '@toolsmithhq/imagecore-types';

export { ImageFormat } from '@toolsmithhq/imagecore-native';
export type { ImageInfo, Rotation, CropRegion, ResizeOptions, EncodeOptions, ExifData } from '@toolsmithhq/imagecore-types';

const CACHE_DIR_NAME = 'imagecore';

function getCacheDir(): Directory {
  const dir = new Directory(Paths.cache, CACHE_DIR_NAME);
  if (!dir.exists) dir.create();
  return dir;
}

/** Decode base64 string to ArrayBuffer */
function base64ToArrayBuffer(base64: string): ArrayBuffer {
  const binaryString = atob(base64);
  const bytes = new Uint8Array(binaryString.length);
  for (let i = 0; i < binaryString.length; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  return bytes.buffer;
}

/** Encode ArrayBuffer to base64 string */
function arrayBufferToBase64(data: ArrayBuffer): string {
  const bytes = new Uint8Array(data);
  let binary = '';
  for (let i = 0; i < bytes.length; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary);
}

/** Get file extension for a format */
export function formatToExtension(format: ImageFormat | string): string {
  switch (format) {
    case 'jpeg': return 'jpg';
    case 'png':  return 'png';
    case 'webp': return 'webp';
    case 'heic': return 'heic';
    case 'avif': return 'avif';
    case 'tiff': return 'tiff';
    case 'bmp':  return 'bmp';
    case 'gif':  return 'gif';
    default:     return 'bin';
  }
}

/** Read a file URI into an ArrayBuffer */
export async function readFileAsArrayBuffer(uri: string): Promise<ArrayBuffer> {
  const file = new File(uri);
  const base64 = await file.base64();
  return base64ToArrayBuffer(base64);
}

/** Write an ArrayBuffer to a cache file and return the URI */
export function writeArrayBufferToFile(
  data: ArrayBuffer,
  extension: string,
): string {
  const dir = getCacheDir();
  const fileName = `${Date.now()}-${Math.random().toString(36).substring(2, 8)}.${extension}`;
  const file = new File(dir, fileName);
  file.create();
  file.write(arrayBufferToBase64(data), { encoding: 'base64' });
  return file.uri;
}

/** Convert a file from one format to another */
export async function convertFile(
  uri: string,
  targetFormat: ImageFormat | string,
  quality: number = 0.85,
): Promise<string> {
  const inputBuffer = await readFileAsArrayBuffer(uri);
  try {
    const outputBuffer = ImageCore.convert(inputBuffer, {
      format: targetFormat as ImageFormat,
      quality,
    });
    return writeArrayBufferToFile(outputBuffer, formatToExtension(targetFormat));
  } catch (e) {
    console.warn(`[imagecore] convert failed (${targetFormat}):`, e);
    throw e;
  }
}

/** Get image info from a file URI. Uses multiple fallbacks. */
export async function getFileImageInfo(uri: string): Promise<ImageInfo> {
  // Try 1: imagecore native (works for JPEG, PNG, WebP, BMP, TIFF, HEIC via platform)
  try {
    const buffer = await readFileAsArrayBuffer(uri);
    const info = ImageCore.getImageInfo(buffer);
    if (info.width > 0 && info.height > 0) return info;
  } catch { /* fall through */ }

  // Try 2: imagecore decode (slower but works for any decodable format)
  try {
    const buffer = await readFileAsArrayBuffer(uri);
    const decoded = ImageCore.decode(buffer);
    const result: ImageInfo = {
      width: decoded.width,
      height: decoded.height,
      format: (uri.split('.').pop()?.toLowerCase() ?? 'unknown') as ImageFormat,
      hasExif: false,
      fileSize: 0,
    };
    decoded.free();
    return result;
  } catch { /* fall through */ }

  // Try 3: RN Image.getSize (works for formats the platform supports natively)
  try {
    const { Image } = require('react-native');
    const { width, height } = await new Promise<{ width: number; height: number }>(
      (resolve, reject) => {
        Image.getSize(
          uri,
          (w: number, h: number) => resolve({ width: w, height: h }),
          (e: Error) => reject(e),
        );
      },
    );

    if (width > 0 && height > 0) {
      const ext = uri.split('.').pop()?.toLowerCase() ?? '';
      const formatMap: Record<string, string> = {
        jpg: 'jpeg', jpeg: 'jpeg', png: 'png', webp: 'webp',
        heic: 'heic', heif: 'heic', avif: 'avif', tiff: 'tiff',
        tif: 'tiff', bmp: 'bmp', gif: 'gif',
      };
      return {
        width,
        height,
        format: (formatMap[ext] ?? 'unknown') as ImageFormat,
        hasExif: false,
        fileSize: 0,
      };
    }
  } catch { /* fall through */ }

  // All methods failed
  return { width: 0, height: 0, format: 'unknown' as ImageFormat, hasExif: false, fileSize: 0 };
}

/** Strip EXIF from a file, return new URI */
export async function stripFileExif(uri: string): Promise<string> {
  const buffer = await readFileAsArrayBuffer(uri);
  const info = ImageCore.getImageInfo(buffer);
  const stripped = ImageCore.stripExif(buffer);
  const ext = info.format as string;
  return writeArrayBufferToFile(stripped, ext === 'jpeg' ? 'jpg' : ext);
}

/** Read EXIF data from a file. Returns empty object if no EXIF or unsupported format. */
export async function readFileExif(uri: string) {
  try {
    const buffer = await readFileAsArrayBuffer(uri);
    return ImageCore.readExif(buffer);
  } catch {
    return {};
  }
}

/** Lossless JPEG rotate (90/180/270) — no quality loss */
export async function losslessJpegRotate(
  uri: string,
  rotation: Rotation,
): Promise<string> {
  const buffer = await readFileAsArrayBuffer(uri);
  const rotated = ImageCore.jpegLosslessRotate(buffer, rotation);
  return writeArrayBufferToFile(rotated, 'jpg');
}

/** Resize an image file */
export async function resizeFile(
  uri: string,
  width: number,
  height: number,
  outputFormat: ImageFormat | string,
  quality: number = 0.95,
): Promise<string> {
  const inputBuffer = await readFileAsArrayBuffer(uri);
  const decoded = ImageCore.decode(inputBuffer);

  try {
    const resized = ImageCore.resize(decoded, { width, height, filter: 'lanczos' });
    try {
      const encoded = ImageCore.encode(resized, {
        format: outputFormat as ImageFormat,
        quality,
      });
      return writeArrayBufferToFile(encoded, formatToExtension(outputFormat));
    } finally {
      resized.free();
    }
  } finally {
    decoded.free();
  }
}

/** Crop an image file */
export async function cropFile(
  uri: string,
  x: number,
  y: number,
  width: number,
  height: number,
  outputFormat: ImageFormat | string,
  quality: number = 0.95,
): Promise<string> {
  const inputBuffer = await readFileAsArrayBuffer(uri);
  const decoded = ImageCore.decode(inputBuffer);

  try {
    const cropped = ImageCore.crop(decoded, { x, y, width, height });
    try {
      const encoded = ImageCore.encode(cropped, {
        format: outputFormat as ImageFormat,
        quality,
      });
      return writeArrayBufferToFile(encoded, formatToExtension(outputFormat));
    } finally {
      cropped.free();
    }
  } finally {
    decoded.free();
  }
}

/** Rotate an image file (90/180/270) */
export async function rotateFile(
  uri: string,
  rotation: Rotation,
  outputFormat: ImageFormat | string,
  quality: number = 0.95,
): Promise<string> {
  const inputBuffer = await readFileAsArrayBuffer(uri);
  const decoded = ImageCore.decode(inputBuffer);

  try {
    const rotated = ImageCore.rotate(decoded, rotation);
    try {
      const encoded = ImageCore.encode(rotated, {
        format: outputFormat as ImageFormat,
        quality,
      });
      return writeArrayBufferToFile(encoded, formatToExtension(outputFormat));
    } finally {
      rotated.free();
    }
  } finally {
    decoded.free();
  }
}

/** Flip an image file horizontally, vertically, or both */
export async function flipFile(
  uri: string,
  horizontal: boolean,
  vertical: boolean,
  outputFormat: ImageFormat | string,
  quality: number = 0.95,
): Promise<string> {
  const inputBuffer = await readFileAsArrayBuffer(uri);
  let current = ImageCore.decode(inputBuffer);

  try {
    if (horizontal) {
      const flipped = ImageCore.flipHorizontal(current);
      current.free();
      current = flipped;
    }
    if (vertical) {
      const flipped = ImageCore.flipVertical(current);
      current.free();
      current = flipped;
    }

    const encoded = ImageCore.encode(current, {
      format: outputFormat as ImageFormat,
      quality,
    });
    return writeArrayBufferToFile(encoded, formatToExtension(outputFormat));
  } finally {
    current.free();
  }
}

/** Compress an image file (re-encode at lower quality) */
export async function compressFile(
  uri: string,
  quality: number,
  outputFormat: ImageFormat | string,
): Promise<string> {
  const inputBuffer = await readFileAsArrayBuffer(uri);
  const outputBuffer = ImageCore.convert(inputBuffer, {
    format: outputFormat as ImageFormat,
    quality,
  });
  return writeArrayBufferToFile(outputBuffer, formatToExtension(outputFormat));
}
