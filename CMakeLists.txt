cmake_minimum_required(VERSION 3.30...4.0)

project(otfccxx
  HOMEPAGE_URL "https://github.com/InCom-0/otfccxx"
  VERSION "0.0.0"
  DESCRIPTION
  "Library for programmatic modifications of OpenType fonts. Focused on font subsetting and modifications of glyph sizes and advance width."
  LANGUAGES CXX C)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

option(otfccxx_BUILD_DEMOS "Build demos for otfccxx" ${PROJECT_IS_TOP_LEVEL})
option(otfccxx_BUILD_SHARED_LIB "Build a shared version of otfccxx" ${BUILD_SHARED_LIBS})


#####################################################################
### Staging ###
#####################################################################
include(cmake/StageOutputs.cmake)
if(otfccxx_STAGE_OUTPUTS)
    stageOutputDirsFor(otfccxx)
endif()


########################################################
### Target specification ###
########################################################
include(CMake_dependencies.cmake) ### Loads and declares the required external libraries using CPM or CMake's FetchContent.

if(otfccxx_BUILD_SHARED_LIB)
  add_library(otfccxx SHARED)
else()
  add_library(otfccxx STATIC)
endif()
add_library(otfccxx::otfccxx ALIAS otfccxx)

target_sources(otfccxx PRIVATE src/otfccxx.cpp src/fmem_file.cpp src/machinery_stderr_capt.cpp)
target_sources(otfccxx
  PUBLIC
  FILE_SET pub_headers
  TYPE HEADERS
  BASE_DIRS
  include)

target_sources(otfccxx
  PRIVATE
  FILE_SET priv_headers
  TYPE HEADERS
  BASE_DIRS
  src/private_inc)

target_link_libraries(otfccxx PRIVATE harfbuzz-subset harfbuzz woff2enc woff2dec otfcc_lib::otfcc_lib fmem::fmem base64)

target_compile_features(otfccxx PRIVATE cxx_std_23)
set_target_properties(otfccxx PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(otfccxx_BUILD_SHARED_LIB)
  set_target_properties(otfccxx PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
  )
  target_compile_definitions(otfccxx
    PRIVATE OTFCCXX_EXPORTS
    PUBLIC OTFCCXX_SHARED
  )
endif()


########################################################
### Demos specification ###
########################################################
if(otfccxx_BUILD_DEMOS)
  add_executable(scratch demos/demo_1.cpp)
  target_compile_features(scratch PRIVATE cxx_std_23)
  target_link_libraries(scratch PRIVATE otfccxx)
  if(USING_LIBSTDCXX)
    target_link_libraries(scratch PRIVATE "-lstdc++exp")
  endif()
endif()


#####################################################################
### Platform specific hacks ###
#####################################################################
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
  add_compile_options(/utf-8)
  add_compile_definitions(NOMINMAX)
endif()

# When we build harfbuzz ourselves we need don't install it and therefore need to use different directory structure for includes
target_compile_definitions(otfccxx
  PRIVATE $<$<BOOL:${OTFCCXX_HARFBUZZ_BUILDFROMSOURCE}>:OTFCCXX_HARFBUZZ_BUILDFROMSOURCE>
)


#####################################################################
### Installing, packaging ###
#####################################################################
if(BUILD_SHARED_LIBS)
  install(
    TARGETS otfccxx
    EXPORT otfccxx_target
    RUNTIME_DEPENDENCY_SET otfccxx_depset
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET pub_headers
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  # When compling with MinGW we need to include the runtime DLLs along
  if(MINGW)
    get_filename_component(OTFCCXX_CXX_COMPILER_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    install(
      RUNTIME_DEPENDENCY_SET otfccxx_depset
      PRE_EXCLUDE_REGEXES
      [[api-ms-.*]]
      [[ext-ms-.*]]
      POST_INCLUDE_REGEXES
      [[libstdc\+\+*]]
      [[libgcc_s*]]
      [[libwoff2*]]
      [[libharfbuzz*]]
      POST_EXCLUDE_REGEXES
      [[\\*.dll]]
      DIRECTORIES
      ${OTFCCXX_CXX_COMPILER_DIR}
    )
  endif()

  # Creating a script for being able to use 'find_package')
  install(
    EXPORT otfccxx_target
    NAMESPACE otfccxx::
    DESTINATION lib/cmake/otfccxx
  )

else()
  message(STATUS "Installing otfccxx is not available because it is being built as STATIC library")
endif(BUILD_SHARED_LIBS)

if(PROJECT_IS_TOP_LEVEL AND BUILD_SHARED_LIBS)
  add_subdirectory(cmake/packaging)
endif()

