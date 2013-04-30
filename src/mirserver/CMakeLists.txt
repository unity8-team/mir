set(SOURCES ubuntu_application_api_mirserver.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++0x")

include_directories(
  ${MIR_SERVER_INCLUDE_DIRS}
)

add_library(
	ubuntu_application_api_mirserver SHARED
	${SOURCES}
)

target_link_libraries(
	ubuntu_application_api_mirserver
        ${MIR_SERVER_LDFLAGS} ${MIR_SERVER_LIBRARIES}
)

set_target_properties(
	ubuntu_application_api_mirserver
	PROPERTIES
	VERSION 1.0
	SOVERSION 1
)

install(TARGETS ubuntu_application_api_mirserver ${INSTALL_TARGETS_DEFAULT_ARGS})

