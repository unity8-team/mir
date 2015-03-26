set(
  SHELL_SOURCES

  abstract_shell.cpp
  default_configuration.cpp
  default_placement_strategy.cpp
  default_window_manager.cpp
  frontend_shell.cpp
  graphics_display_layout.cpp
  shell_wrapper.cpp
  skeleton_window_manager.cpp
)

add_library(
  mirshell OBJECT

  ${SHELL_SOURCES}
)
