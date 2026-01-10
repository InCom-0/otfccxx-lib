cmake_minimum_required(VERSION 3.30...4.0)

project(otfccxx-lib
  HOMEPAGE_URL "https://github.com/InCom-0/otfccxx-lib"
  VERSION "0.0.0"
  DESCRIPTION
  "Library for programmatic modifications of OpenType fonts. Focused on font subsetting and modifications of glyph sizes and advance width."
  LANGUAGES CXX C)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option(OTFCCXX-LIB_BUILD_DEMOS "Build demos of otfccxx-lib" ${PROJECT_IS_TOP_LEVEL})
# set(BUILD_SHARED_LIBS OFF)


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
target_sources(otfccxx-lib PRIVATE src/otfccxx-lib.cpp src/fmem_file.cpp src/machinery_stderr_capt.cpp)
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

target_link_libraries(otfccxx-lib PRIVATE otfcc_lib::otfcc_lib harfbuzz-subset harfbuzz fmem::fmem woff2enc woff2dec)

set_target_properties(otfccxx-lib PROPERTIES POSITION_INDEPENDENT_CODE TRUE)


########################################################
### Demos specification ###
########################################################
if(OTFCCXX-LIB_BUILD_DEMOS)
  add_executable(scratch demos/demo_1.cpp)
  target_compile_features(scratch PRIVATE cxx_std_23)
  target_link_libraries(scratch PRIVATE otfccxx-lib)
endif()


