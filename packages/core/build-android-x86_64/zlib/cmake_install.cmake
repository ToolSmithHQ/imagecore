# Install script for directory: /Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/Users/ombhardwaj/Library/Android/sdk/ndk/28.0.12674087/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libz.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libz.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libz.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/libz.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libz.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libz.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Users/ombhardwaj/Library/Android/sdk/ndk/28.0.12674087/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libz.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib/ZLIB-shared.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib/ZLIB-shared.cmake"
         "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/CMakeFiles/Export/93f1ef598f1f2f8b07b376ab081bbce6/ZLIB-shared.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib/ZLIB-shared-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib/ZLIB-shared.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/CMakeFiles/Export/93f1ef598f1f2f8b07b376ab081bbce6/ZLIB-shared.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/CMakeFiles/Export/93f1ef598f1f2f8b07b376ab081bbce6/ZLIB-shared-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/libz.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib/ZLIB-static.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib/ZLIB-static.cmake"
         "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/CMakeFiles/Export/93f1ef598f1f2f8b07b376ab081bbce6/ZLIB-static.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib/ZLIB-static-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib/ZLIB-static.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/CMakeFiles/Export/93f1ef598f1f2f8b07b376ab081bbce6/ZLIB-static.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/CMakeFiles/Export/93f1ef598f1f2f8b07b376ab081bbce6/ZLIB-static-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/zlib" TYPE FILE FILES
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/ZLIBConfig.cmake"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/ZLIBConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/zconf.h"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/zlib.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Docs" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man3" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/zlib.3")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Docs" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/zlib/zlib" TYPE FILE FILES
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/LICENSE"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/doc/algorithm.txt"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/doc/crc-doc.1.0.pdf"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/doc/rfc1950.txt"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/doc/rfc1951.txt"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/doc/rfc1952.txt"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/zlib/doc/txtvsbin.txt"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/zlib.pc")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/test/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/contrib/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/zlib/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
