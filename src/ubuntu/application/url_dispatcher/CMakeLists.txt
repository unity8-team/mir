find_package(PkgConfig)
pkg_check_modules(LIBURL_DISPATCHER REQUIRED url-dispatcher-1)

include_directories(${LIBURL_DISPATCHER_INCLUDE_DIRS})
link_directories(${LIBURL_DISPATCHER_LIBRARY_DIRS})

add_library(
  ubuntu_application_url_dispatcher
  SHARED

  service.cpp
  session.cpp
)

target_link_libraries(
  ubuntu_application_url_dispatcher

  ${LIBURL_DISPATCHER_LIBRARIES}
  -Wl,--no-undefined
)

install(
  TARGETS ubuntu_application_url_dispatcher
  DESTINATION ${LIB_INSTALL_DIR}
)
