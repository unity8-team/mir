if (MIR_BUILD_PLATFORM_MESA_KMS)
  add_subdirectory(kms/)
endif()
if (MIR_BUILD_PLATFORM_MESA_X11)
  add_subdirectory(x11/)
endif()

include_directories(
    ${server_common_include_dirs}
    ${DRM_INCLUDE_DIRS}
)

add_library(
  mirsharedmesaservercommon-static STATIC

  buffer_allocator.cpp
  display_helpers.cpp
  drm_close_threadsafe.cpp
  gbm_buffer.cpp
  ipc_operations.cpp
  software_buffer.cpp
  drm_native_platform.cpp
)

target_link_libraries(
  mirsharedmesaservercommon-static
  mirsharedmesa-static

  server_platform_common
  kms_utils
)
