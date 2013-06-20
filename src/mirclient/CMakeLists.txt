set(SOURCES 
  ubuntu_application_api_mirclient.cpp
  application_instance_mirclient.cpp
  window_properties_mirclient.cpp
  window_mirclient.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

add_library(
	ubuntu_application_api_mirclient SHARED
	${SOURCES}
)

include_directories(
    ${CMAKE_BINARY_DIR}/include
    ${MIRCLIENT_INCLUDE_DIRS}
)

target_link_libraries(
	ubuntu_application_api_mirclient
        ${MIRCLIENT_LDFLAGS} ${MIRCLIENT_LIBRARIES}
        -Wl,--whole-archive ubuntu_application_api_mircommon
        -Wl,--no-whole-archive
)

set_target_properties(
	ubuntu_application_api_mirclient
	PROPERTIES
	VERSION 1.0
	SOVERSION 1
)

install(TARGETS ubuntu_application_api_mirclient ${INSTALL_TARGETS_DEFAULT_ARGS})
