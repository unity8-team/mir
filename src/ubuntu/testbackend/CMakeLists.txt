set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

add_library(
  ubuntu_application_test_api SHARED
  ubuntu_application_sensors.cpp
)

set_target_properties(
  ubuntu_application_test_api
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

install(
  TARGETS ubuntu_application_test_api
  ${INSTALL_TARGETS_DEFAULT_ARGS}
)

