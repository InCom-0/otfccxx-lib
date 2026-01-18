include(cmake/CPM.cmake)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

if(NOT CPM_USE_LOCAL_PACKAGES)
  set(CPM_USE_LOCAL_PACKAGES ON)
endif()


CPMAddPackage(
  NAME harfbuzz
  URL https://github.com/harfbuzz/harfbuzz/releases/download/12.3.0/harfbuzz-12.3.0.tar.xz
  URL_HASH SHA256=8660ebd3c27d9407fc8433b5d172bafba5f0317cb0bb4339f28e5370c93d42b7
  EXCLUDE_FROM_ALL TRUE
  OPTIONS "BUILD_SHARED_LIBS ON" "HB_BUILD_UTILS OFF"
)
CPMAddPackage(
  URI "gh:InCom-0/otfcc-lib_cmake#master"
  OPTIONS "BUILD_SHARED_LIBS OFF"
)
CPMAddPackage(
  URI "gh:InCom-0/fmem#master"
  OPTIONS "FMEM_STATIC ON" "ALLOW_OPENMEMSTREAM OFF"
)
CPMAddPackage(
  URI "gh:InCom-0/woff2#otfccxx"
  OPTIONS "NOISY_LOGGING OFF" "BUILD_SHARED_LIBS OFF"
  FIND_PACKAGE_ARGUMENTS NAME WOFF2
)
CPMAddPackage(
  URI "gh:tobiaslocker/base64#master"
  NAME base64tl
)



if(harfbuzz_ADDED)
  set(OTFCCXX_HARFBUZZ_BUILDFROMSOURCE TRUE)
endif()

if(otfcc-lib_cmake_ADDED)
  set(OTFCCXX_OTFCC-LIB_CMAKE_BUILDFROMSOURCE TRUE)
endif()

if(fmem_ADDED)
  set(OTFCCXX_FMEM_BUILDFROMSOURCE TRUE)
endif()

if(woff2_ADDED)
  set(OTFCCXX_WOFF2_BUILDFROMSOURCE TRUE)
endif()

if(base64tl_ADDED)
  set(OTFCCXX_BASE64TL_BUILDFROMSOURCE TRUE)
endif()
