set(
  UBUNTU_HARDWARE_API_LINK_LIBRARIES
    
  ubuntu_hardware_alarm
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

include_directories(
  ${CMAKE_BINARY_DIR}/include
)

include_directories(../../bridge)

add_subdirectory(alarms/)

add_library(
  ubuntu_platform_hardware_api SHARED
  ubuntu_platform_hardware_api.cpp
)

target_link_libraries(
  ubuntu_platform_hardware_api

  "-Wl,--whole-archive"
  ${UBUNTU_HARDWARE_API_LINK_LIBRARIES}
  "-Wl,--no-whole-archive"

  dl
  ${Hybris}
)

set_target_properties(
  ubuntu_platform_hardware_api
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

install(
  TARGETS ubuntu_platform_hardware_api
  ${INSTALL_TARGETS_DEFAULT_ARGS}
)

add_subdirectory(tests/)
