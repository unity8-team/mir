include_directories(
    ${LIBHARDWARE_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLESv2_INCLUDE_DIRS}
)

add_library(
  mirandroidgraphicsbuffer STATIC

  android_buffer_allocator.cpp
  buffer.cpp
  android_buffer_handle.cpp
  android_alloc_adaptor.cpp
)

target_link_libraries(
  mirandroidgraphicsbuffer

  ${LIBHARDWARE_LIBRARIES}
  ${EGL_LDFLAGS} ${EGL_LIBRARIES}
  ${GLESv2_LDFLAGS} ${GLESv2_LIBRARIES}
)
