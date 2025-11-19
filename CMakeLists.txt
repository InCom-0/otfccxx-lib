cmake_minimum_required(VERSION 3.30...4.0)

project(otfccxx-lib
  HOMEPAGE_URL "https://github.com/InCom-0/otfccxx-lib"
  VERSION "0.0.0"
  DESCRIPTION
  "Library for programmatic modifying and/or splicing (combining or merging) OpenType fonts. Provides granual control on a 'per glyph' or 'per codepoint' basis."
  LANGUAGES CXX C)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

#####################################################################
### Platform specific hacks + LTO Check ###
#####################################################################
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
  add_compile_options(/utf-8)
  add_compile_definitions(NOMINMAX)
endif()

########################################################
### Target specification ###
########################################################
add_library(otfccxx-lib)
target_sources(otfccxx-lib PRIVATE src/otfccxx-lib.cpp)
target_sources(otfccxx-lib
  PUBLIC
  FILE_SET pub_headers
  TYPE HEADERS
  BASE_DIRS
  include)

target_sources(otfccxx-lib
  PRIVATE
  FILE_SET priv_headers
  TYPE HEADERS
  BASE_DIRS
  src/private_inc)

target_compile_features(otfccxx-lib PRIVATE cxx_std_23)

include(CMake_dependencies.cmake) ### Loads and declares the required external libraries using CPM or CMake's FetchContent.

# target_link_libraries(incplot PRIVATE incplot-lib::incplot-lib argparse::argparse)

### This attempts to compile sample program to find out if we are using libc++(USING_LIBCXX) or libstdc++(USING_LIBSTDCXX)
include(cmake/stdlibQuery.cmake)
if(USING_LIBSTDCXX)
  target_link_libraries(otfccxx-lib PRIVATE "-lstdc++exp")
endif()