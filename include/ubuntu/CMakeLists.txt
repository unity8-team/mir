set(
  UBUNTU_HEADERS
  status.h
  unit.h
  visibility.h
)

install(
  FILES ${UBUNTU_HEADERS}
  DESTINATION include/ubuntu
)

add_subdirectory(application)
add_subdirectory(hardware)
add_subdirectory(ui)