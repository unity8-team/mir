configure_file(
  ubuntu-platform-api.pc.in ubuntu-platform-api.pc @ONLY
)

configure_file(
  ubuntu-platform-hardware-api.pc.in ubuntu-platform-hardware-api.pc @ONLY
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/ubuntu-platform-api.pc ${CMAKE_CURRENT_BINARY_DIR}/ubuntu-platform-hardware-api.pc
  DESTINATION lib/${CMAKE_LIBRARY_ARCHITECTURE}/pkgconfig
)
