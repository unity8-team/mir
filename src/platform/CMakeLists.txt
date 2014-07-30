include_directories(${PROJECT_SOURCE_DIR}/include/platform)

add_definitions(-DMIR_SERVER_PLATFORM_PLUGIN_PATH="${MIR_SERVER_PLATFORM_PLUGIN_PATH}")

add_subdirectory(graphics/)
add_subdirectory(options)
