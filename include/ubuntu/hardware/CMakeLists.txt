set(
  UBUNTU_HARDWARE_HEADERS
  gps.h
)

install(
  FILES ${UBUNTU_HARDWARE_HEADERS}
  DESTINATION include/ubuntu/hardware
)