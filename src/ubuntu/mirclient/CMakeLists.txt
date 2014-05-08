protobuf_generate_cpp(
  GENERATED_PROTOBUF_SRCS GENERATED_PROTOBUF_HDRS
  unityrpc.proto
)

set(SOURCES
  ubuntu_application_api_mirclient.cpp
  application_instance_mirclient.cpp
  window_properties_mirclient.cpp
  window_mirclient.cpp
  ${GENERATED_PROTOBUF_SRCS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

add_library(
	ubuntu_application_api_mirclient SHARED
	${SOURCES}
)

include_directories(
    ${CMAKE_BINARY_DIR}/include
    ${MIRCLIENT_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(
  ubuntu_application_api_mirclient
  ${MIRCLIENT_LDFLAGS} ${MIRCLIENT_LIBRARIES}
  -Wl,--whole-archive ubuntu_application_api_mircommon ${UBUNTU_APPLICATION_API_LINK_LIBRARIES} -Wl,--no-whole-archive
  ${PROTOBUF_LIBRARIES}
)

set_target_properties(
	ubuntu_application_api_mirclient
	PROPERTIES
	VERSION 1.0
	SOVERSION 1
)

install(TARGETS ubuntu_application_api_mirclient ${INSTALL_TARGETS_DEFAULT_ARGS})
