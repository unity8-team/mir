include_directories(${GL_INCLUDE_DIRS})

set(
  GRAPHICS_SOURCES

  egl_extensions.cpp
  egl_resources.cpp
  egl_error.cpp
  display_configuration.cpp
  gamma_curves.cpp
  buffer_basic.cpp
  pixel_format_utils.cpp
  overlapping_output_grouping.cpp
  platform_probe.cpp
  atomic_frame.cpp
  ${PROJECT_SOURCE_DIR}/include/platform/mir/graphics/display.h
)

add_library(mirplatformgraphicscommon OBJECT

  ${GRAPHICS_SOURCES}
)

set(symbol_map ${CMAKE_CURRENT_SOURCE_DIR}/symbols.map)
