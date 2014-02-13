find_package(PkgConfig)
find_package(Threads)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

pkg_check_modules(DBUS_CPP REQUIRED dbus-cpp)
pkg_check_modules(LOCATION_SERVICE REQUIRED ubuntu-location-service)

include_directories(
  ${DBUS_CPP_INCLUDE_DIRS}
  ${LOCATION_SERVICE_INCLUDE_DIRS}
)

add_library(
  ubuntu_application_location

  controller.cpp
  service.cpp
  session.cpp

  heading_update.cpp
  position_update.cpp
  velocity_update.cpp
)

target_link_libraries(
  ubuntu_application_location

  ${LOCATION_SERVICE_LDFLAGS}
  ${DBUS_CPP_LDFLAGS}
  ${CMAKE_THREAD_LIBS_INIT}
)
