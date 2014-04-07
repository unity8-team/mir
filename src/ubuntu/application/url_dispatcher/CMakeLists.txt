find_package(PkgConfig)
pkg_check_modules(LIBURL_DISPATCHER REQUIRED url-dispatcher-1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

include_directories(${LIBURL_DISPATCHER_INCLUDE_DIRS})

add_library(
  ubuntu_application_url_dispatcher

  service.cpp
  session.cpp
)

target_link_libraries(
  ubuntu_application_url_dispatcher

  ${LIBURL_DISPATCHER_LDFLAGS}
)
