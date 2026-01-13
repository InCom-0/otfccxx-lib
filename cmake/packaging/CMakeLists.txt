if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  set(CMAKE_SYSTEM_PROCESSOR arm64)
endif()

set(CPACK_PACKAGE_NAME "otfccxx-lib")
set(CPACK_PACKAGE_VENDOR "incom")
set(CPACK_PACKAGE_FILE_NAME "otfccxx-lib")
set(CPACK_PACKAGE_DIRECTORY ${PROJECT_BINARY_DIR})

# From the GitHub About section
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Library for programmatic modifications of OpenType fonts. Focused on font subsetting and modifications of glyph sizes and advance width.")

# set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})

# Pull the versions defined with the top level CMakeLists.txt
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)

# CPACK_VERBATIM_VARIABLES ensures that the variables prefixed with *CPACK_*
# are correctly passed to the cpack program.
# This should always be set to true.
set(CPACK_VERBATIM_VARIABLES TRUE)

set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE.txt)
set(CPACK_RESOURCE_FILE_README ${PROJECT_SOURCE_DIR}/README.md)


if(MINGW)
  if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(CPACK_PACKAGE_FILE_NAME "otfccxx-lib_windows_gnu_x86_64")
  endif()
  if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "ARM64")
    set(CPACK_PACKAGE_FILE_NAME "otfccxx-lib_windows_gnu_arm64")
  endif()

  set(CPACK_GENERATOR ZIP)
elseif(MSVC)
  if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(CPACK_PACKAGE_FILE_NAME "otfccxx-lib_windows_msvc_x86_64")
  endif()
  if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "ARM64")
    set(CPACK_PACKAGE_FILE_NAME "otfccxx-lib_windows_msvc_arm64")
  endif()
  set(CPACK_GENERATOR ZIP)


elseif(APPLE)
  set(CPACK_PACKAGE_FILE_NAME "otfccxx-lib_macos_${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_GENERATOR TGZ)
  # set(CPACK_PACKAGE_ICON ${CMAKE_CURRENT_LIST_DIR}/neovim.icns)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_PACKAGE_FILE_NAME "otfccxx-lib_linux_${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_GENERATOR TGZ)
  set(CPACK_DEBIAN_PACKAGE_NAME "otfccxx-lib") # required
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "incom") # required

else()
  set(CPACK_GENERATOR TGZ)
endif()

# CPack variables are loaded in on the call to include(CPack). If you set
# variables *after* the inclusion, they don't get updated within the CPack
# config. Note that some CPack commands should still be run after it, such
# as cpack_add_component().
include(CPack)
