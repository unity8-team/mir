include_directories(${PROJECT_SOURCE_DIR}/include/platform)

add_definitions(-DMIR_SERVER_PLATFORM_PLUGIN_PATH="${MIR_SERVER_PLATFORM_PLUGIN_PATH}")

set(MIRPLATFORM_ABI 1)

add_library(
  mirplatform SHARED

  $<TARGET_OBJECTS:mirplatformgraphicscommon>
  $<TARGET_OBJECTS:miroptions>
  $<TARGET_OBJECTS:mirfatal>
)

target_link_libraries(mirplatform

  mircommon
  ${EGL_LDFLAGS} ${EGL_LIBRARIES}
  ${GLESv2_LDFLAGS} ${GLESv2_LIBRARIES}
)

set_target_properties(
  mirplatform

  PROPERTIES
  SOVERSION ${MIRPLATFORM_ABI}
)

install(TARGETS mirplatform LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_subdirectory(graphics/)
add_subdirectory(options)
add_subdirectory(fatal)
