set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

add_library(
  ubuntu_application_api_test MODULE
 
  ubuntu_application_sensors.cpp
  module.cpp
  test_stubs.cpp
  module_version.h
)

target_link_libraries(
  ubuntu_application_api_test

  rt
)

set_target_properties(
  ubuntu_application_api_test
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

install(
  TARGETS ubuntu_application_api_test
  # this is not a development library to compile against, users should always
  # specify the SONAME; so don't build a *.so
  LIBRARY DESTINATION "${LIB_INSTALL_DIR}" NAMELINK_SKIP
)

