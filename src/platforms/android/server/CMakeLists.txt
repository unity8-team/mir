include_directories(
    ${PROJECT_SOURCE_DIR}/src/include/gl
)

include_directories(
    ${EGL_INCLUDE_DIRS}
    ${GLESv2_INCLUDE_DIRS}
    ${ANDROID_PROPERTIES_INCLUDE_DIRS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

add_library(mirplatformgraphicsandroidobjects OBJECT
  platform.cpp
  graphic_buffer_allocator.cpp
  buffer.cpp
  display.cpp
  display_group.cpp
  display_configuration.cpp
  display_buffer.cpp
  hal_component_factory.cpp
  hwc_layerlist.cpp
  hwc_layers.cpp
  hwc_fb_device.cpp
  hwc_loggers.cpp
  hwc_device.cpp
  gralloc_module.cpp
  server_render_window.cpp
  resource_factory.cpp
  framebuffers.cpp
  fb_device.cpp
  interpreter_cache.cpp
  gl_context.cpp
  device_quirks.cpp
  real_hwc_wrapper.cpp
  hwc_fallback_gl_renderer.cpp
  ipc_operations.cpp
  hwc_blanking_control.cpp
  egl_sync_factory.cpp
  virtual_output.cpp
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/symbols.map.in
    ${CMAKE_CURRENT_BINARY_DIR}/symbols.map)
set(symbol_map ${CMAKE_CURRENT_BINARY_DIR}/symbols.map)

add_library(mirplatformgraphicsandroid SHARED
  $<TARGET_OBJECTS:mirplatformgraphicsandroidobjects>
  $<TARGET_OBJECTS:mirgl>
)

target_link_libraries(
  mirplatformgraphicsandroid

  mirsharedandroid-static
  mirplatform
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${LIBHARDWARE_LIBRARIES}
  ${EGL_LDFLAGS} ${EGL_LIBRARIES}
  ${GLESv2_LDFLAGS} ${GLESv2_LIBRARIES}
  ${ANDROID_PROPERTIES_LDFLAGS}
)

set_target_properties(
  mirplatformgraphicsandroid PROPERTIES
  OUTPUT_NAME graphics-android
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/server-modules
  PREFIX ""
  SUFFIX ".so.${MIR_SERVER_GRAPHICS_PLATFORM_ABI}"
  LINK_FLAGS "-Wl,--exclude-libs=ALL -Wl,--version-script,${symbol_map}"
  LINK_DEPENDS ${symbol_map}
)

install(TARGETS mirplatformgraphicsandroid LIBRARY DESTINATION ${MIR_SERVER_PLATFORM_PATH})
