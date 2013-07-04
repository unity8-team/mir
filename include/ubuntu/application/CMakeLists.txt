set(
  UBUNTU_APPLICATION_HEADERS
  archive.h
  description.h
  id.h
  instance.h
  lifecycle_delegate.h
  operation_mode.h
  options.h
  ubuntu_application_gps.h
)

install(
  FILES ${UBUNTU_APPLICATION_HEADERS}
  DESTINATION include/ubuntu-${UBUNTU_PLATFORM_API_VERSION_MAJOR}/application
)

add_subdirectory(sensors)
add_subdirectory(ui)