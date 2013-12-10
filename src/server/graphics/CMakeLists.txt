include_directories(${GLESv2_INCLUDE_DIRS})

add_library(
  mirgraphics STATIC

  default_configuration.cpp
  default_output_configuration.cpp
  default_output_configuration.h
  default_display_configuration_policy.cpp
  default_display_configuration_policy.h
)

add_subdirectory(nested/)
add_subdirectory(offscreen/)
