cmake_minimum_required(VERSION 2.6)

project(ubuntu-platform-api)

set(UBUNTU_PLATFORM_API_VERSION_MAJOR 2)
set(UBUNTU_PLATFORM_API_VERSION_MINOR 2)
set(UBUNTU_PLATFORM_API_VERSION_PATCH 0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

if (CMAKE_BUILD_TYPE MATCHES [cC][oO][vV][eE][rR][aA][gG][eE])
  set(CMAKE_GCOV gcov)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage -fprofile-arcs" )
  SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -ftest-coverage -fprofile-arcs" )
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -ftest-coverage -fprofile-arcs" )
endif (CMAKE_BUILD_TYPE MATCHES [cC][oO][vV][eE][rR][aA][gG][eE])

set(BIN_INSTALL_DIR bin)
set(LIB_INSTALL_DIR lib/${CMAKE_LIBRARY_ARCHITECTURE})

set(INSTALL_TARGETS_DEFAULT_ARGS RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
                                 LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
                                 ARCHIVE DESTINATION "${LIB_INSTALL_DIR}")

#### Detect if we have got gles2, and fall back to plain gl if not ####
set(UBUNTU_PLATFORM_API_HAVE_GLES2 0)

find_path(
  GLES2_INCLUDE_PATH
  GLES2/gl2.h
)

if (GLES2_INCLUDE_PATH)
  message(STATUS "GLES2 found in ${GLES2_INCLUDE_PATH}")
  set(UBUNTU_PLATFORM_API_HAVE_GLES2 1)
else (GLES2_INCLUDE_PATH)
  message(STATUS "GLES2 not found, falling back to plain GL")
endif (GLES2_INCLUDE_PATH)
#### Detect if we have got gles2, and fall back to plain gl if not ####

#### Detect if we have EGL/egl.h, and bail out if not ####
find_path(
  EGL_INCLUDE_PATH
  EGL/egl.h
)

if (NOT EGL_INCLUDE_PATH)
  message(ERROR EGL not found)
else (NOT EGL_INCLUDE_PATH)
  message(STATUS "EGL found in ${EGL_INCLUDE_PATH}")
endif (NOT EGL_INCLUDE_PATH)
#### Detect if we have EGL/egl.h, and bail out if not ####

option(
  ENABLE_HYBRIS_IMPLEMENTATION
  "Enable hybris based platform API implementation"
  ON
)

find_package(PkgConfig)
pkg_check_modules(MIRSERVER REQUIRED mirserver)
pkg_check_modules(MIRCLIENT REQUIRED mirclient)

find_package(Protobuf REQUIRED)
if(PROTOBUF_PROTOC_EXECUTABLE STREQUAL "PROTOBUF_PROTOC_EXECUTABLE-NOTFOUND")
  message(SEND_ERROR "protoc executable not found! Missing protobuf-compiler package?")
endif()

# Try to find hybris, and disable hybris from build if not found
find_library(Hybris
  NAMES hybris-common
)

if(NOT Hybris)
  message(STATUS "Notice: libhybris-common.so not found, disabling hybris support")
  set(ENABLE_HYBRIS_IMPLEMENTATION OFF)
endif()

include_directories(include)
include_directories(android/include)

add_subdirectory(data/)
add_subdirectory(doc/)
add_subdirectory(include/)
add_subdirectory(src/)
add_subdirectory(examples/)

#### Enable tests
include(CTest)
set(GTEST_ROOT /usr/src/gtest)
add_subdirectory(tests/)
add_subdirectory(${GTEST_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gtest)
