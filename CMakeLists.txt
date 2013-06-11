cmake_minimum_required(VERSION 2.6)

project(ubuntu-platform-api)

set(UBUNTU_PLATFORM_API_VERSION_MAJOR 0)
set(UBUNTU_PLATFORM_API_VERSION_MINOR 0)
set(UBUNTU_PLATFORM_API_VERSION_PATCH 1)

set(BIN_INSTALL_DIR bin)
set(LIB_INSTALL_DIR lib/${CMAKE_LIBRARY_ARCHITECTURE})

set(INSTALL_TARGETS_DEFAULT_ARGS RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
                                 LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
                                 ARCHIVE DESTINATION "${LIB_INSTALL_DIR}")

#### Detect if we have got gles2, and fall back to plain gl if not ####
set(UBUNTU_PLATFORM_API_HAVE_GLES2 0)

find_path(
  GLES2_INCLUDE_PATH
  gles2/gl2.h
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

include_directories(include)
include_directories(android/include)

add_subdirectory(data/)
add_subdirectory(doc/)
add_subdirectory(include/)
add_subdirectory(src/)
