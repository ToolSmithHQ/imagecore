/**
 * @toolsmithhq/imagecore-types
 *
 * Shared types for @toolsmithhq/imagecore-native and @toolsmithhq/imagecore-web.
 * Both packages implement the same ImageCore interface.
 */

export enum ImageFormat {
  JPEG = 'jpeg',
  PNG = 'png',
  WEBP = 'webp',
  HEIC = 'heic',
  AVIF = 'avif',
  TIFF = 'tiff',
  BMP = 'bmp',
  GIF = 'gif',
}

export interface ImageInfo {
  width: number;
  height: number;
  format: ImageFormat;
  hasExif: boolean;
  fileSize: number;
}

export interface EncodeOptions {
  /** Target format */
  format: ImageFormat;
  /** Quality 0.0–1.0, for lossy formats (JPEG, WebP lossy, AVIF, HEIC). Default: 0.85 */
  quality?: number;
  /** Enable lossless mode (WebP, AVIF). Default: false */
  lossless?: boolean;
  /** Strip metadata on encode. Default: false */
  stripExif?: boolean;
}

export type ResizeFilter = 'lanczos' | 'bilinear' | 'nearest';

export interface ResizeOptions {
  width: number;
  height: number;
  /** Resize algorithm. Default: 'lanczos' */
  filter?: ResizeFilter;
}

export type Rotation = 90 | 180 | 270;

export interface CropRegion {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface ExifData {
  [key: string]: string | number | boolean | undefined;
}

/* ── Decoded image handle ────────────────────────────────────────────── */

/**
 * Opaque handle to a decoded image in memory (RGBA pixels).
 * Created by decode(), consumed by encode()/resize()/crop()/rotate().
 * Must be freed when no longer needed.
 */
export interface DecodedImage {
  readonly width: number;
  readonly height: number;
  /** Release the underlying native/WASM memory. */
  free(): void;
}

/* ── Core API interface ──────────────────────────────────────────────── */

/**
 * The ImageCore API, implemented identically by both
 * @toolsmithhq/imagecore-native (JSI) and @toolsmithhq/imagecore-web (WASM).
 */
export interface ImageCore {

  getImageInfo(data: ArrayBuffer): ImageInfo;

  /** Decode any supported format to RGBA pixels. */
  decode(data: ArrayBuffer): DecodedImage;

  /** Encode decoded image to the specified format. */
  encode(image: DecodedImage, options: EncodeOptions): ArrayBuffer;

  /* ── Lossless JPEG  ─────────────────────────────────────────────────── */

  /**
   * Lossless JPEG rotation (90/180/270).
   * Rearranges DCT blocks — zero quality loss, no pixel decode.
   */
  jpegLosslessRotate(data: ArrayBuffer, rotation: Rotation): ArrayBuffer;

  /**
   * Lossless JPEG crop at MCU boundaries.
   * Region is aligned to nearest MCU boundary automatically.
   */
  jpegLosslessCrop(data: ArrayBuffer, region: CropRegion): ArrayBuffer;

  /**
   * Strip EXIF from JPEG by binary removal of APP1 segment.
   * No re-encoding. Output is always smaller than input.
   */
  jpegStripExif(data: ArrayBuffer): ArrayBuffer;

  /* ── Pixel operations ──────────────────────────────────────────────── */

  /** Resize decoded image. */
  resize(image: DecodedImage, options: ResizeOptions): DecodedImage;

  /** Crop decoded image. */
  crop(image: DecodedImage, region: CropRegion): DecodedImage;

  /** Rotate decoded image by 90/180/270 degrees. */
  rotate(image: DecodedImage, rotation: Rotation): DecodedImage;

  /** Flip decoded image horizontally (mirror). Returns new image. */
  flipHorizontal(image: DecodedImage): DecodedImage;

  /** Flip decoded image vertically. Returns new image. */
  flipVertical(image: DecodedImage): DecodedImage;

  /* ── Metadata ──────────────────────────────────────────────────────── */

  /** Read EXIF data from an encoded image. */
  readExif(data: ArrayBuffer): ExifData;

  /**
   * Strip EXIF/metadata from any format.
   * Uses binary stripping for JPEG (no re-encode), decode+encode for others.
   */
  stripExif(data: ArrayBuffer): ArrayBuffer;

  /* ── Convenience ───────────────────────────────────────────────────── */

  /**
   * Convert image from one format to another in a single call.
   * Equivalent to decode() + encode().
   */
  convert(data: ArrayBuffer, options: EncodeOptions): ArrayBuffer;
}
