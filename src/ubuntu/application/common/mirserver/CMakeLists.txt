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

target_link_libraries(
	ubuntu_application_api_mirserver

    ${MIRSERVER_LDFLAGS} ${MIRSERVER_LIBRARIES}
)
