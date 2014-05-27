set(
  UBUNTU_APPLICATION_SENSORS_HEADERS
  accelerometer.h
  light.h
  proximity.h
  haptic.h
  orientation.h
)

install(
  FILES ${UBUNTU_APPLICATION_SENSORS_HEADERS}
  DESTINATION include/ubuntu/application/sensors
)

add_subdirectory(event)
