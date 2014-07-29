include_directories(${PROJECT_SOURCE_DIR}/include/platform)
include_directories(${UDEV_INCLUDE_DIRS})

add_definitions(-DMIR_SERVER_PLATFORM_PLUGIN_PATH="${MIR_SERVER_PLATFORM_PLUGIN_PATH}")

add_library(
  mirplatform SHARED
  udev_wrapper.cpp
)

target_link_libraries(
  mirplatform
  mirplatformgraphicscommon
  mirsharedsharedlibrary

  ${UDEV_LDFLAGS} ${UDEV_LIBRARIES}
)

install(TARGETS mirplatform LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_subdirectory(graphics/)
add_subdirectory(options)
