set(
  UBUNTU_APPLICATION_API_LINK_LIBRARIES

  ubuntu_application_sensors_haptic
  ubuntu_application_location
  ubuntu_application_url_dispatcher
)

include_directories(
  ${CMAKE_BINARY_DIR}/include
)

include_directories(../../bridge)

add_subdirectory(common)
add_subdirectory(desktop)
add_subdirectory(touch)
add_subdirectory(testbackend)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

add_library(
  ubuntu_application_api SHARED
  
  ubuntu_application_api.cpp
)

target_link_libraries(
  ubuntu_application_api

  dl  
)

set_target_properties(
  ubuntu_application_api
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

install(
  TARGETS ubuntu_application_api
  ${INSTALL_TARGETS_DEFAULT_ARGS}
)
