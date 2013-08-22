set(
  UBUNTU_APPLICATION_URL_DISPATCHER_HEADERS

  service.h
  session.h
)

install(
  FILES ${UBUNTU_APPLICATION_URL_DISPATCHER_HEADERS}
  DESTINATION include/ubuntu/application/url_dispatcher
)
