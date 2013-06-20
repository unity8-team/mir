set(SOURCES 
  hybris_bridge.cpp
  ubuntu_application_api_hybris.cpp
  ubuntu_application_sensors_hybris.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++0x")

add_library(
  ubuntu_application_api SHARED
  ${SOURCES}
)

include_directories(
  ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(
  ubuntu_application_api
  hybris-common
)

set_target_properties(
  ubuntu_application_api
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

install(
  TARGETS ubuntu_application_api  
  ${INSTALL_TARGETS_DEFAULT_ARGS})

add_subdirectory(tests/)
