cmake_minimum_required(VERSION 3.30...4.0)

project(otfccxx-lib
  HOMEPAGE_URL "https://github.com/InCom-0/otfccxx-lib"
  VERSION "0.0.0"
  DESCRIPTION
  "Library for programmatic modifications of OpenType fonts. Focused on font subsetting and modifications of glyph sizes and advance width."
  LANGUAGES CXX C)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(BUILD_SHARED_LIBS OFF)

#####################################################################
### Platform specific hacks ###
#####################################################################
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
  add_compile_options(/utf-8)
  add_compile_definitions(NOMINMAX)
endif()

########################################################
### Target specification ###
########################################################
add_library(otfccxx-lib)
target_sources(otfccxx-lib PRIVATE src/otfccxx-lib.cpp src/fmem_file.cpp)
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

target_link_libraries(otfccxx-lib PRIVATE otfcc-lib::otfcc-lib harfbuzz-subset fmem::fmem woff2enc woff2dec)

### This attempts to compile sample program to find out if we are using libc++(USING_LIBCXX) or libstdc++(USING_LIBSTDCXX)
# include(cmake/stdlibQuery.cmake)
# if(USING_LIBSTDCXX)
#   target_link_libraries(otfccxx-lib PRIVATE "-lstdc++exp")
# endif()


add_executable(scratch demos/demo_1.cpp)
target_compile_features(scratch PRIVATE cxx_std_23)
target_link_libraries(scratch PRIVATE otfccxx-lib)
