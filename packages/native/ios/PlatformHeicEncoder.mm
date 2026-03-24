#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "PlatformHeicEncoder.h"

#include <cstdlib>
#include <cstring>

extern "C" int platform_heic_decode(const uint8_t* data, size_t len, ICImage* out) {
    @autoreleasepool {
        NSData* nsData = [NSData dataWithBytesNoCopy:(void*)data length:len freeWhenDone:NO];
        CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)nsData, nil);
        if (!source) return IC_ERROR_DECODE_FAILED;

        CGImageRef cgImage = CGImageSourceCreateImageAtIndex(source, 0, nil);
        CFRelease(source);
        if (!cgImage) return IC_ERROR_DECODE_FAILED;

        uint32_t width = (uint32_t)CGImageGetWidth(cgImage);
        uint32_t height = (uint32_t)CGImageGetHeight(cgImage);

        out->width = width;
        out->height = height;
        out->stride = width * 4;

        size_t total = (size_t)out->stride * height;
        out->pixels = (uint8_t*)malloc(total);
        if (!out->pixels) { CGImageRelease(cgImage); return IC_ERROR_ALLOC_FAILED; }

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            out->pixels, width, height, 8, out->stride, colorSpace,
            kCGImageAlphaPremultipliedLast | (CGBitmapInfo)kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(colorSpace);

        if (!ctx) { CGImageRelease(cgImage); free(out->pixels); out->pixels = nullptr; return IC_ERROR_DECODE_FAILED; }

        CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), cgImage);
        CGContextRelease(ctx);
        CGImageRelease(cgImage);

        // Unpremultiply alpha
        for (size_t i = 0; i < total; i += 4) {
            uint8_t a = out->pixels[i + 3];
            if (a > 0 && a < 255) {
                out->pixels[i + 0] = (uint8_t)((out->pixels[i + 0] * 255) / a);
                out->pixels[i + 1] = (uint8_t)((out->pixels[i + 1] * 255) / a);
                out->pixels[i + 2] = (uint8_t)((out->pixels[i + 2] * 255) / a);
            }
        }
        return IC_OK;
    }
}

extern "C" int platform_heic_encode(const ICImage* img, const ICEncodeOpts* opts,
                                     uint8_t** out_data, size_t* out_len) {
    @autoreleasepool {
        // Check if image has any non-opaque pixels
        bool hasAlpha = false;
        size_t total = (size_t)img->stride * img->height;
        for (size_t i = 3; i < total; i += 4) {
            if (img->pixels[i] != 255) { hasAlpha = true; break; }
        }

        CGBitmapInfo bitmapInfo = (CGBitmapInfo)kCGBitmapByteOrder32Big |
            (hasAlpha ? kCGImageAlphaPremultipliedLast : kCGImageAlphaNoneSkipLast);

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            img->pixels, img->width, img->height, 8, img->stride, colorSpace,
            bitmapInfo);
        CGColorSpaceRelease(colorSpace);
        if (!ctx) return IC_ERROR_ENCODE_FAILED;

        CGImageRef cgImage = CGBitmapContextCreateImage(ctx);
        CGContextRelease(ctx);
        if (!cgImage) return IC_ERROR_ENCODE_FAILED;

        NSMutableData* outputData = [NSMutableData data];
        CGImageDestinationRef dest = CGImageDestinationCreateWithData(
            (__bridge CFMutableDataRef)outputData,
            (__bridge CFStringRef)UTTypeHEIC.identifier, 1, nil);
        if (!dest) { CGImageRelease(cgImage); return IC_ERROR_ENCODE_FAILED; }

        float quality = opts ? opts->quality : 0.85f;
        NSDictionary* properties = @{
            (__bridge NSString*)kCGImageDestinationLossyCompressionQuality: @(quality)
        };
        CGImageDestinationAddImage(dest, cgImage, (__bridge CFDictionaryRef)properties);

        bool success = CGImageDestinationFinalize(dest);
        CFRelease(dest);
        CGImageRelease(cgImage);
        if (!success) return IC_ERROR_ENCODE_FAILED;

        *out_len = outputData.length;
        *out_data = (uint8_t*)malloc(*out_len);
        if (!*out_data) return IC_ERROR_ALLOC_FAILED;
        memcpy(*out_data, outputData.bytes, *out_len);
        return IC_OK;
    }
}

extern "C" int platform_heic_get_info(const uint8_t* data, size_t len, ICImageInfo* out) {
    @autoreleasepool {
        NSData* nsData = [NSData dataWithBytesNoCopy:(void*)data length:len freeWhenDone:NO];
        CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)nsData, nil);
        if (!source) return IC_ERROR_DECODE_FAILED;

        CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil);
        if (!properties) { CFRelease(source); return IC_ERROR_DECODE_FAILED; }

        CFNumberRef widthRef = (CFNumberRef)CFDictionaryGetValue(properties, kCGImagePropertyPixelWidth);
        CFNumberRef heightRef = (CFNumberRef)CFDictionaryGetValue(properties, kCGImagePropertyPixelHeight);

        int width = 0, height = 0;
        if (widthRef) CFNumberGetValue(widthRef, kCFNumberIntType, &width);
        if (heightRef) CFNumberGetValue(heightRef, kCFNumberIntType, &height);

        out->width = (uint32_t)width;
        out->height = (uint32_t)height;
        out->format = IC_FORMAT_HEIC;

        CFDictionaryRef exifDict = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyExifDictionary);
        out->has_exif = (exifDict != nil) ? 1 : 0;

        CFRelease(properties);
        CFRelease(source);
        return IC_OK;
    }
}
