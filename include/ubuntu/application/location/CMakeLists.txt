set(
  UBUNTU_APPLICATION_SENSORS_HEADERS

  heading_update.h
  position_update.h
  service.h
  session.h
  velocity_update.h
)

install(
  FILES ${UBUNTU_APPLICATION_SENSORS_HEADERS}
  DESTINATION include/ubuntu-${UBUNTU_PLATFORM_API_VERSION_MAJOR}/application/location
)