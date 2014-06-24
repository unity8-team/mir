set(
  UBUNTU_APPLICATION_SENSORS_EVENT_HEADERS
  accelerometer.h
  light.h
  proximity.h
  orientation.h
)

install(
  FILES ${UBUNTU_APPLICATION_SENSORS_EVENT_HEADERS}
  DESTINATION include/ubuntu/application/sensors/event
)
