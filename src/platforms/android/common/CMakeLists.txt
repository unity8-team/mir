include_directories(
    ${server_common_include_dirs}
    include/
)

add_definitions( -DANDROID )

add_library(
  mirsharedandroid-static STATIC

  mir_native_window.cpp
  refcounted_buffer.cpp
  android_native_buffer.cpp
  syncfence.cpp
  egl_sync_fence.cpp
  egl_sync_extensions.cpp
  native_window_report.cpp
)
