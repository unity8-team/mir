set(SOURCES
  ubuntu_application_api_mirserver.cpp
)

include_directories(
    ${CMAKE_BINARY_DIR}/include
)

add_library(
    ubuntu_application_api_mirserver STATIC
    ${SOURCES}
)

target_link_libraries(
    ubuntu_application_api_mirserver
)
