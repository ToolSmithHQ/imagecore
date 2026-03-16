require 'json'
package = JSON.parse(File.read(File.join(__dir__, '..', 'package.json')))

Pod::Spec.new do |s|
  s.name         = 'ImageCore'
  s.version      = package['version']
  s.summary      = package['description']
  s.homepage     = 'https://github.com/toolsmith-labs/imagecore'
  s.license      = package['license']
  s.author       = 'Toolsmith'
  s.platforms    = { :ios => '15.0' }
  s.source       = { :git => 'https://github.com/toolsmith-labs/imagecore.git', :tag => s.version }

  s.dependency 'React-Core'
  s.dependency 'React-callinvoker'
  s.dependency 'React-cxxreact'

  s.source_files = [
    'ImageCore.mm',
    'ImageCoreModule.h',
    'ImageCoreHostObject.{h,cpp}',
    'PlatformHeicEncoder.{h,mm}',
  ]

  s.preserve_paths = 'prebuilt/**'

  s.script_phase = {
    :name => 'Select ImageCore Prebuilt Library',
    :script => 'PDIR="${PODS_TARGET_SRCROOT}/prebuilt"; if [ "${PLATFORM_NAME}" = "iphonesimulator" ]; then cp -f "$PDIR/simulator/libimagecore-all.a" "$PDIR/libimagecore-all.a"; else cp -f "$PDIR/device/libimagecore-all.a" "$PDIR/libimagecore-all.a"; fi',
    :execution_position => :before_compile
  }

  s.vendored_libraries = 'prebuilt/libimagecore-all.a'

  s.pod_target_xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
    'HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)/prebuilt/include"',
    'OTHER_LDFLAGS' => '-lc++',
  }

  s.frameworks = 'ImageIO', 'CoreGraphics', 'UniformTypeIdentifiers'
end
