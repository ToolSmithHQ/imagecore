# Install script for directory: /Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/simd/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/src/md5/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "lib" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/libjpeg.a")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib" TYPE STATIC_LIBRARY FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/libjpeg.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "bin" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/cjpeg")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin" TYPE PROGRAM RENAME "cjpeg" FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/cjpeg-static")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "bin" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/djpeg")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin" TYPE PROGRAM RENAME "djpeg" FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/djpeg-static")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "bin" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/jpegtran")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin" TYPE PROGRAM RENAME "jpegtran" FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/jpegtran-static")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "bin" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/rdjpgcom" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/rdjpgcom")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/rdjpgcom"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/rdjpgcom")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin" TYPE EXECUTABLE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/rdjpgcom")
  if(EXISTS "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/rdjpgcom" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/rdjpgcom")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Users/ombhardwaj/Library/Android/sdk/ndk/28.0.12674087/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip" "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/rdjpgcom")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "bin" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/wrjpgcom" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/wrjpgcom")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/wrjpgcom"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/wrjpgcom")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin" TYPE EXECUTABLE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/wrjpgcom")
  if(EXISTS "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/wrjpgcom" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/wrjpgcom")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Users/ombhardwaj/Library/Android/sdk/ndk/28.0.12674087/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip" "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/bin/wrjpgcom")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "doc" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo/README.ijg;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo/README.md;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo/example.c;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo/libjpeg.txt;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo/structure.txt;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo/usage.txt;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo/wizard.txt;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo/LICENSE.md")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/doc/libjpeg-turbo" TYPE FILE FILES
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/README.ijg"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/README.md"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/src/example.c"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/libjpeg.txt"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/structure.txt"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/usage.txt"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/wizard.txt"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/LICENSE.md"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "man" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/man/man1/cjpeg.1;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/man/man1/djpeg.1;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/man/man1/jpegtran.1;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/man/man1/rdjpgcom.1;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/man/man1/wrjpgcom.1")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/share/man/man1" TYPE FILE FILES
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/cjpeg.1"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/djpeg.1"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/jpegtran.1"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/rdjpgcom.1"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/doc/wrjpgcom.1"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "lib" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/pkgconfig/libjpeg.pc")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/pkgconfig" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/pkgscripts/libjpeg.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "lib" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo/libjpeg-turboConfig.cmake;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo/libjpeg-turboConfigVersion.cmake")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo" TYPE FILE FILES
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/pkgscripts/libjpeg-turboConfig.cmake"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/pkgscripts/libjpeg-turboConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "lib" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo/libjpeg-turboTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo/libjpeg-turboTargets.cmake"
         "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/CMakeFiles/Export/27562ff9b7f9bb1140c23230f66f748d/libjpeg-turboTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo/libjpeg-turboTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo/libjpeg-turboTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo/libjpeg-turboTargets.cmake")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/CMakeFiles/Export/27562ff9b7f9bb1140c23230f66f748d/libjpeg-turboTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
     "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo/libjpeg-turboTargets-release.cmake")
    if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
      message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
    endif()
    if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
      message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
    endif()
    file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/lib/cmake/libjpeg-turbo" TYPE FILE FILES "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/CMakeFiles/Export/27562ff9b7f9bb1140c23230f66f748d/libjpeg-turboTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "include" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/include/jconfig.h;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/include/jerror.h;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/include/jmorecfg.h;/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/include/jpeglib.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/install/include" TYPE FILE FILES
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/jconfig.h"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/src/jerror.h"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/src/jmorecfg.h"
    "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/third_party/libjpeg-turbo/src/jpeglib.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/install_local_manifest.txt"
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
  file(WRITE "/Users/ombhardwaj/Development/projects/aiotool/imagecore/packages/core/build-android-x86_64/libjpeg-turbo/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
