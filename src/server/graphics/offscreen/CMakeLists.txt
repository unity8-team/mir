add_library(
  miroffscreengraphics STATIC

  display.cpp
  display_configuration.cpp
  display_buffer.cpp
  dummy_egl_surface.cpp
  gl_extensions_base.cpp
)

target_link_libraries(
  miroffscreengraphics
)
