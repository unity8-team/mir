set(SOURCES
  ubuntu_application_api_mircommon.cpp
  application_id_mir.cpp
  application_options_mir.cpp
  application_description_mir.cpp
  lifecycle_delegate_mir.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

include_directories(
  ${MIRCOMMON_INCLUDE_DIRS}
)

add_library(
	ubuntu_application_api_mircommon STATIC
	${SOURCES}
)

target_link_libraries(
	ubuntu_application_api_mircommon
        ${MIR_COMMON_LDFLAGS} ${MIR_COMMON_LIBRARIES}
)


