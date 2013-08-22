set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

add_library(
  ubuntu_application_api SHARED

  ubuntu_application_api_hybris.cpp
  ubuntu_application_sensors_hybris.cpp
)

add_library(
  ubuntu_platform_hardware_api SHARED
  ubuntu_platform_hardware_api.cpp
)

include_directories(
  ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(
  ubuntu_application_api

  "-Wl,--whole-archive"
  ${UBUNTU_APPLICATION_API_LINK_LIBRARIES}  
  "-Wl,--no-whole-archive"
  hybris-common
)

target_link_libraries(
  ubuntu_platform_hardware_api
  hybris-common
)

set_target_properties(
  ubuntu_application_api
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

set_target_properties(
  ubuntu_platform_hardware_api
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

install(
  TARGETS ubuntu_application_api  
  ${INSTALL_TARGETS_DEFAULT_ARGS}
)

install(
  TARGETS ubuntu_platform_hardware_api
  ${INSTALL_TARGETS_DEFAULT_ARGS}
)

add_subdirectory(tests/)
