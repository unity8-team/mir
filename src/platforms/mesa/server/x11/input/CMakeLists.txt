include_directories(
  ${PROJECT_SOURCE_DIR}/include/platform
  ${PROJECT_SOURCE_DIR}/include/client
)

add_library(
  mirplatforminputmesax11objects OBJECT

  input.cpp
  input_platform.cpp
  input_device.cpp
  dispatchable.cpp
)
