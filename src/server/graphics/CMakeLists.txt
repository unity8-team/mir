include_directories(${GLESv2_INCLUDE_DIRS})

add_library(
  mirgraphics STATIC

  default_configuration.cpp
  default_display_configuration_policy.cpp
  default_display_configuration_policy.h
  pixel_format_utils.h
  pixel_format_utils.cpp
)

add_subdirectory(nested/)
add_subdirectory(offscreen/)
