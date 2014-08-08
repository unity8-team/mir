include_directories(${PROJECT_SOURCE_DIR}/include/platform)

# This needs to change whenever libmirplatform.so's ABI changes in an
# incompatible way
set(MIRPLATFORM_ABI 1)

# This needs to change whenever the much smaller ABI between the platform
# modules and the server changes in an incompatible way.
set(MIR_PLATFORM_PLUGIN_ABI 1)

set(MIR_SERVER_PLATFORM_PLUGIN_PATH
  ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/mir/server-platform-${MIR_PLATFORM_PLUGIN_ABI}
)
set(MIR_SERVER_PLATFORM_PLUGIN_PATH
  ${MIR_SERVER_PLATFORM_PLUGIN_PATH}
  PARENT_SCOPE
)

add_definitions(-DMIR_SERVER_PLATFORM_PLUGIN_PATH="${MIR_SERVER_PLATFORM_PLUGIN_PATH}")

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
