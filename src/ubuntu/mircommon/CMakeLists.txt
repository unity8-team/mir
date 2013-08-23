set(SOURCES
  event_helpers_mir.cpp
  application_id_mir.cpp
  application_options_mir.cpp
  application_description_mir.cpp
  session_mir.cpp
  lifecycle_delegate_mir.cpp
  )

set(LINK_LIBRARIES
  ${MIR_COMMON_LDFLAGS} ${MIR_COMMON_LIBRARIES}
  )

if(ENABLE_HYBRIS_IMPLEMENTATION)
  list(APPEND SOURCES
    ../hybris/ubuntu_application_sensors_hybris.cpp
    )
  list(APPEND LINK_LIBRARIES
    hybris-common
    )
else()
  list(APPEND SOURCES
    ubuntu_application_sensors_desktop.cpp
    )
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC --std=c++11")

include_directories(
  ${MIRCOMMON_INCLUDE_DIRS}
  )

add_library(
  ubuntu_application_api_mircommon STATIC
  ${SOURCES}
  )

target_link_libraries(
  ubuntu_application_api_mircommon

  ${UBUNTU_APPLICATION_API_LINK_LIBRARIES}
  ${LINK_LIBRARIES}
  )
