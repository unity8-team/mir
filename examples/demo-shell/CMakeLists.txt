add_executable(mir_demo_shell
  demo_shell.cpp
  fullscreen_placement_strategy.cpp
  application_switcher.cpp
  software_cursor_compositing_strategy.cpp
)

target_link_libraries(mir_demo_shell
  mirserver
)
