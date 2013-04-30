set(
  GRAPHICS_SOURCES

  gl_renderer.cpp
)

add_library(
  mirgraphics STATIC

  ${GRAPHICS_SOURCES}
)

target_link_libraries(
  mirgraphics

  mirlogging

  ${MIR_COMMON_PLATFORM_LIBRARIES}
  ${GLESv2_LIBRARIES}
)
