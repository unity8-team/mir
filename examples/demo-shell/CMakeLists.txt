add_executable(mir_demo_server_shell
  demo_shell.cpp
  fullscreen_placement_strategy.h
  fullscreen_placement_strategy.cpp
  cascaded_display_configuration_policy.h
  cascaded_display_configuration_policy.cpp
  translucent_outputs.h
  translucent_outputs.cpp
  window_manager.h
  window_manager.cpp
  ../server_configuration.h
  ../server_configuration.cpp
)

target_link_libraries(mir_demo_server_shell
  mirserver
)

install(TARGETS mir_demo_server_shell RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
