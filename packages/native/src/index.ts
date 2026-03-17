/**
 * @toolsmith/imagecore-native
 *
 * React Native image processing via JSI.
 * Same API as @toolsmith/imagecore-web — import from whichever matches your platform.
 *
 * Usage:
 *   import { ImageCore } from '@toolsmith/imagecore-native';
 *   const rotated = ImageCore.jpegLosslessRotate(buffer, 90);
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
} from '@toolsmith/imagecore-types';

export type {
  ImageInfo,
  EncodeOptions,
  DecodedImage,
  Rotation,
  CropRegion,
  ResizeOptions,
  ExifData,
};

export { ImageFormat } from '@toolsmith/imagecore-types';

import { NativeModules } from 'react-native';

/**
 * Install the JSI host object by calling the native install() method.
 * On iOS: installs the C++ JSI HostObject on global.__ImageCoreProxy.
 * On Android: no-op (Android doesn't use JSI yet — operations throw until NDK build is integrated).
 */
let _installed = false;
function ensureInstalled(): void {
  if (_installed) return;

  const nativeModule = NativeModules.ImageCore;
  if (!nativeModule) {
    throw new Error(
      '@toolsmith/imagecore-native: NativeModule not found. ' +
      'Make sure you have rebuilt the app (not Expo Go).'
    );
  }

  const result = nativeModule.install();
  if (!result) {
    throw new Error(
      '@toolsmith/imagecore-native: install() failed — bridge.runtime is nil.'
    );
  }

  _installed = true;
}

/**
 * Get the native JSI module.
 * On iOS: returns global.__ImageCoreProxy (C++ JSI HostObject).
 * On Android: throws — NDK build not yet integrated.
 */
function getNativeModule(): IImageCore {
  ensureInstalled();

  // @ts-expect-error — global installed by JSI host object
  const mod = global.__ImageCoreProxy;
  if (!mod) {
    throw new Error(
      '@toolsmith/imagecore-native: __ImageCoreProxy not found after install(). ' +
      'Make sure you have rebuilt the app with npx expo prebuild --clean.'
    );
  }
  return mod;
}

/**
 * The ImageCore API — synchronous, runs on the JS thread via JSI.
 *
 * All methods accept/return ArrayBuffer for zero-copy data transfer.
 */
export const ImageCore: IImageCore = {
  getImageInfo(data: ArrayBuffer): ImageInfo {
    return getNativeModule().getImageInfo(data);
  },

  decode(data: ArrayBuffer): DecodedImage {
    return getNativeModule().decode(data);
  },

  encode(image: DecodedImage, options: EncodeOptions): ArrayBuffer {
    return getNativeModule().encode(image, options);
  },

  jpegLosslessRotate(data: ArrayBuffer, rotation: Rotation): ArrayBuffer {
    return getNativeModule().jpegLosslessRotate(data, rotation);
  },

  jpegLosslessCrop(data: ArrayBuffer, region: CropRegion): ArrayBuffer {
    return getNativeModule().jpegLosslessCrop(data, region);
  },

  jpegStripExif(data: ArrayBuffer): ArrayBuffer {
    return getNativeModule().jpegStripExif(data);
  },

  resize(image: DecodedImage, options: ResizeOptions): DecodedImage {
    return getNativeModule().resize(image, options);
  },

  crop(image: DecodedImage, region: CropRegion): DecodedImage {
    return getNativeModule().crop(image, region);
  },

  rotate(image: DecodedImage, rotation: Rotation): DecodedImage {
    return getNativeModule().rotate(image, rotation);
  },

  flipHorizontal(image: DecodedImage): DecodedImage {
    return getNativeModule().flipHorizontal(image);
  },

  flipVertical(image: DecodedImage): DecodedImage {
    return getNativeModule().flipVertical(image);
  },

  readExif(data: ArrayBuffer): ExifData {
    return getNativeModule().readExif(data);
  },

  stripExif(data: ArrayBuffer): ArrayBuffer {
    return getNativeModule().stripExif(data);
  },

  convert(data: ArrayBuffer, options: EncodeOptions): ArrayBuffer {
    return getNativeModule().convert(data, options);
  },
};
