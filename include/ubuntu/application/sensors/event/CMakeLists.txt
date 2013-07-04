set(
  UBUNTU_APPLICATION_SENSORS_EVENT_HEADERS
  accelerometer.h
  light.h
  proximity.h
)

install(
  FILES ${UBUNTU_APPLICATION_SENSORS_EVENT_HEADERS}
  DESTINATION include/ubuntu-${UBUNTU_PLATFORM_API_VERSION_MAJOR}/application/sensors/event
)