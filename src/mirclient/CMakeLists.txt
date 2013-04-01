set(SOURCES ubuntu_application_api_mirclient.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++0x")

add_library(
	ubuntu_application_api_mirclient SHARED
	${SOURCES}
)

target_link_libraries(
	ubuntu_application_api_mirclient
        mirclient
)

set_target_properties(
	ubuntu_application_api_mirclient
	PROPERTIES
	VERSION 1.0
	SOVERSION 1
)

install(TARGETS ubuntu_application_api_mirclient ${INSTALL_TARGETS_DEFAULT_ARGS})

