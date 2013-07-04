set(
  UBUNTU_HEADERS
  status.h
  unit.h
)

install(
  FILES ${UBUNTU_HEADERS}
  DESTINATION include/ubuntu-${UBUNTU_PLATFORM_API_VERSION_MAJOR}
)

add_subdirectory(application)
add_subdirectory(ui)