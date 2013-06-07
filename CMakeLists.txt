cmake_minimum_required(VERSION 2.6)

project(ubuntu_platform_api)

set(BIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/bin)
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE})

set(INSTALL_TARGETS_DEFAULT_ARGS RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
                                 LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
                                 ARCHIVE DESTINATION "${LIB_INSTALL_DIR}")

option(
  ENABLE_HYBRIS_IMPLEMENTATION
  "Enable hybris based platform API implementation"
  ON
)

option(
  ENABLE_MIRSERVER_IMPLEMENTATION
  "Enable mirserver based platorm API implementation"
  OFF
)

if(ENABLE_MIRSERVER_IMPLEMENTATION)
  find_package(PkgConfig)
  pkg_check_modules(MIRSERVER REQUIRED mirserver)
endif(ENABLE_MIRSERVER_IMPLEMENTATION)

option(
  ENABLE_MIRCLIENT_IMPLEMENTATION
  "Enable mirclient based platform API implementation"
  OFF
)

if(ENABLE_MIRCLIENT_IMPLEMENTATION)
  find_package(PkgConfig)
  pkg_check_modules(MIRCLIENT REQUIRED mirclient)
endif(ENABLE_MIRCLIENT_IMPLEMENTATION)

if(ENABLE_MIRCLIENT_IMPLEMENTATION OR ENABLE_MIRSERVER_IMPLEMENTATION)
  pkg_check_modules(MIRCOMMON REQUIRED mircommon)
endif()

# Sanity-check options
if(NOT DEFINED USE_GLES)
  set(USE_GLES 1)
endif()

# Try to find hybris, and disable hybris from build if not found
find_library(Hybris
  NAMES hybris_ics
)

if(NOT Hybris)
  message("Notice: libhybris_ics.so not found, disabling hybris support")
  set(ENABLE_HYBRIS_IMPLEMENTATION OFF)
endif()

include_directories(include)
include_directories(android/include)

add_subdirectory(src/)
add_subdirectory(include/)
