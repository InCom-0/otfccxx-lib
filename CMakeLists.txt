cmake_minimum_required(VERSION 3.30...4.0)

project(otfccxx-lib
  HOMEPAGE_URL "https://github.com/InCom-0/otfccxx-lib"
  VERSION "0.0.0"
  DESCRIPTION
  "Library for programmatic modifications of OpenType fonts. Focused on font subsetting and modifications of glyph sizes and advance width."
  LANGUAGES CXX C)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

option(OTFCCXX-LIB_BUILD_DEMOS "Build demos of otfccxx-lib" ${PROJECT_IS_TOP_LEVEL})


#####################################################################
### Staging ###
#####################################################################
include(GNUInstallDirs)

set(OTFCCXX_STAGEDIR ${CMAKE_CURRENT_BINARY_DIR}/stage)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${OTFCCXX_STAGEDIR}/${CMAKE_INSTALL_BINDIR}
  )
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${OTFCCXX_STAGEDIR}/${CMAKE_INSTALL_LIBDIR}
  )
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${OTFCCXX_STAGEDIR}/${CMAKE_INSTALL_LIBDIR}
  )
endif()


########################################################
### Target specification ###
########################################################
include(CMake_dependencies.cmake) ### Loads and declares the required external libraries using CPM or CMake's FetchContent.

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

# target_link_libraries(otfccxx-lib PRIVATE harfbuzz-subset harfbuzz woff2enc woff2dec)
target_link_libraries(otfccxx-lib PRIVATE
  $<BUILD_INTERFACE:harfbuzz-subset>
  $<BUILD_INTERFACE:harfbuzz>
  $<BUILD_INTERFACE:woff2enc>
  $<BUILD_INTERFACE:woff2dec>
  $<BUILD_INTERFACE:otfcc_lib::otfcc_lib>
  $<BUILD_INTERFACE:fmem::fmem>
)

set_target_properties(otfccxx-lib PROPERTIES POSITION_INDEPENDENT_CODE TRUE)


########################################################
### Demos specification ###
########################################################
if(OTFCCXX-LIB_BUILD_DEMOS)
  add_executable(scratch demos/demo_1.cpp)
  target_compile_features(scratch PRIVATE cxx_std_23)
  target_link_libraries(scratch PRIVATE otfccxx-lib)
endif()


#####################################################################
### Platform specific hacks ###
#####################################################################
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
  add_compile_options(/utf-8)
  add_compile_definitions(NOMINMAX)
endif()

# When we build harfbuzz ourselves we need don't install it and therefore need to use different directory strcuture for includes
target_compile_definitions(otfccxx-lib
  PRIVATE $<$<BOOL:${OTFCCXX_HARFBUZZ_BUILDFROMSOURCE}>:OTFCCXX_HARFBUZZ_BUILDFROMSOURCE>
)


#####################################################################
### Installing, packaging ###
#####################################################################
install(
  TARGETS otfccxx-lib
  EXPORT otfccxx_target
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  FILE_SET pub_headers
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Creating a script for being able to use 'find_package')
install(
  EXPORT otfccxx_target
  NAMESPACE otfccxx-lib::
  DESTINATION lib/cmake/otfccxx-lib
)

