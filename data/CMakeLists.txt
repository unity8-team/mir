configure_file(
  ubuntu-platform-api.pc.in ubuntu-platform-api.pc @ONLY
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/ubuntu-platform-api.pc
  DESTINATION lib/pkgconfig
)