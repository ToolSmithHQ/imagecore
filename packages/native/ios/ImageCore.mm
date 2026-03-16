#import "ImageCoreModule.h"
#import <React/RCTBridge+Private.h>

#include "ImageCoreHostObject.h"

@implementation ImageCoreModule {
    BOOL _installed;
}

RCT_EXPORT_MODULE(ImageCore)

@synthesize bridge = _bridge;

+ (BOOL)requiresMainQueueSetup {
    return YES;
}

- (void)invalidate {
    _installed = NO;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
    if (_installed) {
        return @YES;
    }

    RCTCxxBridge *cxxBridge = (RCTCxxBridge *)self.bridge;
    if (!cxxBridge || !cxxBridge.runtime) {
        NSLog(@"[ImageCore] ERROR: bridge.runtime is nil");
        return @NO;
    }

    auto& runtime = *(facebook::jsi::Runtime *)cxxBridge.runtime;
    imagecore::install(runtime);
    _installed = YES;
    NSLog(@"[ImageCore] JSI host object installed successfully");
    return @YES;
}

@end
