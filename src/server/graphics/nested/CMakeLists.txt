include_directories(
    ${PROJECT_SOURCE_DIR}/include/client
    ${PROJECT_SOURCE_DIR}/include/server/mir/scene
    ${PROJECT_SOURCE_DIR}/src/server/scene
)

add_library(
  mirnestedgraphics STATIC

  nested_display.cpp
  nested_display_configuration.cpp
  nested_output.cpp
  nested_platform.cpp
  mir_client_host_connection.cpp
)

target_link_libraries(
  mirnestedgraphics
)
