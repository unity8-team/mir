include_directories(${PROJECT_SOURCE_DIR}/include/platform)

add_definitions(-DMIR_SERVER_PLATFORM_PLUGIN_PATH="${MIR_SERVER_PLATFORM_PLUGIN_PATH}")

add_library(
  mirplatform SHARED
)

target_link_libraries(
  mirplatform
  mirplatformgraphicscommon
  mircommon
)

install(TARGETS mirplatform LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_subdirectory(graphics/)
add_subdirectory(options)
