find_package(PkgConfig)
find_package(Threads)
pkg_check_modules(DBUS_CPP REQUIRED dbus-cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC -pthread")

include_directories(
    ${DBUS_CPP_INCLUDE_DIRS}
    )

add_library(
  ubuntu_application_sensors_haptic

  service.cpp
)

target_link_libraries(
  ubuntu_application_sensors_haptic

  ${DBUS_CPP_LIBRARIES}
)
