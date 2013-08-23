set(
  UBUNTU_APPLICATION_LOCATION_HEADERS

  controller.h
  heading_update.h
  position_update.h
  service.h
  session.h
  velocity_update.h
)

install(
  FILES ${UBUNTU_APPLICATION_LOCATION_HEADERS}
  DESTINATION include/ubuntu/application/location
)
