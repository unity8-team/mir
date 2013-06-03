set(SOURCES ubuntu_application_api_mirserver.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

add_library(
	ubuntu_application_api_mirserver SHARED
	${SOURCES}
)

include_directories(
    ${CMAKE_BINARY_DIR}/include
    ${MIRSERVER_INCLUDE_DIRS}
)

target_link_libraries(
	ubuntu_application_api_mirserver
        ${MIRSERVER_LDFLAGS} ${MIRSERVER_LIBRARIES}
)

set_target_properties(
	ubuntu_application_api_mirserver
	PROPERTIES
	VERSION 1.0
	SOVERSION 1
)

install(TARGETS ubuntu_application_api_mirserver ${INSTALL_TARGETS_DEFAULT_ARGS})
