set(SOURCES
  ubuntu_application_api_mirserver.cpp
  application_instance_mirserver.cpp
  window_properties_mirserver.cpp
  window_mirserver.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

include_directories(
    ${CMAKE_BINARY_DIR}/include
    ${MIRSERVER_INCLUDE_DIRS}
)

add_library(
	ubuntu_application_api_mirserver STATIC
	${SOURCES}
)

#ubuntu_application_api_d_sensors

target_link_libraries(
	ubuntu_application_api_mirserver

    ${MIRSERVER_LDFLAGS} ${MIRSERVER_LIBRARIES}
#    ${MIRCOMMON_LINK}
)

install(FILES ubuntu_application_api_mirserver_priv.h DESTINATION include/ubuntu/application)
