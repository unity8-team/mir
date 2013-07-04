configure_file(
  config.h.in 
  ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

set(
  UBUNTU_APPLICATION_UI_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  clipboard.h
  display.h
  form_factor.h
  options.h
  session.h
  stage.h
  window.h
  window_parent.h
  window_properties.h
  window_type.h
)

install(
  FILES ${UBUNTU_APPLICATION_UI_HEADERS}
  DESTINATION include/ubuntu-${UBUNTU_PLATFORM_API_VERSION_MAJOR}/application/ui
)

add_subdirectory(input)