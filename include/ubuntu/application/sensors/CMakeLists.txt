set(
  UBUNTU_APPLICATION_SENSORS_HEADERS
  accelerometer.h
  light.h
  proximity.h
  ubuntu_application_sensors.h
)

install(
  FILES ${UBUNTU_APPLICATION_SENSORS_HEADERS}
  DESTINATION include/ubuntu-${UBUNTU_PLATFORM_API_VERSION_MAJOR}/application/sensors
)

add_subdirectory(event)