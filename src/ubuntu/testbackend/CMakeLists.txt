set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

add_library(
  ubuntu_application_test_api SHARED
  ubuntu_application_sensors.cpp
)

install(
  TARGETS ubuntu_application_test_api
  ${INSTALL_TARGETS_DEFAULT_ARGS}
)
