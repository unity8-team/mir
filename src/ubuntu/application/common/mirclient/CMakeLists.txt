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

include_directories(
    ${CMAKE_BINARY_DIR}/include
    ${MIRCLIENT_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(
	ubuntu_application_api_mirclient STATIC
	${SOURCES}
)

target_link_libraries(
  ubuntu_application_api_mirclient

  ${MIRCLIENT_LDFLAGS} ${MIRCLIENT_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
)
