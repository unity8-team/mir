set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

add_library(
  ubuntu_application_api_desktop_mirclient MODULE

  module.cpp
  module_version.h
  ubuntu_application_sensors_desktop.cpp
)

add_library(
  ubuntu_application_api_desktop_mirserver MODULE

  module.cpp
  module_version.h
  ubuntu_application_sensors_desktop.cpp
)

target_link_libraries(
  ubuntu_application_api_desktop_mirclient

  "-Wl,--whole-archive"
  ubuntu_application_api_mirclient
  ubuntu_application_api_mircommon
  ${UBUNTU_APPLICATION_API_LINK_LIBRARIES}
  "-Wl,--no-whole-archive"
  #TODO: Alarms
)

target_link_libraries(
  ubuntu_application_api_desktop_mirserver

  "-Wl,--whole-archive"
  ubuntu_application_api_mirserver
  ubuntu_application_api_mircommon
  ${UBUNTU_APPLICATION_API_LINK_LIBRARIES}
  "-Wl,--no-whole-archive"
  #TODO: Alarms
)

set_target_properties(
  ubuntu_application_api_desktop_mirclient
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

set_target_properties(
  ubuntu_application_api_desktop_mirserver
  PROPERTIES
  VERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}.${UBUNTU_PLATFORM_API_VERSION_MINOR}.${UBUNTU_PLATFORM_API_VERSION_PATCH}
  SOVERSION ${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)
install(
  TARGETS ubuntu_application_api_desktop_mirclient
  # this is not a development library to compile against, users should always
  # specify the SONAME; so don't build a *.so
  LIBRARY DESTINATION "${LIB_INSTALL_DIR}" NAMELINK_SKIP
)

install(
  TARGETS ubuntu_application_api_desktop_mirserver
  # this is not a development library to compile against, users should always
  # specify the SONAME; so don't build a *.so
  LIBRARY DESTINATION "${LIB_INSTALL_DIR}" NAMELINK_SKIP
)
