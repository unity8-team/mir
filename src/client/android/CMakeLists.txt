include_directories(SYSTEM ${LIBHARDWARE_INCLUDE_DIRS})
add_definitions(-DANDROID)

set(ANDROID_PLATFORM_SRC

  buffer.cpp
  android_client_buffer_factory.cpp
  gralloc_registrar.cpp
  android_client_platform.cpp
  client_platform_factory.cpp
  client_surface_interpreter.cpp
  android_native_display_container.cpp
)  

add_library(
  mirclientplatformandroid MODULE

  ${ANDROID_PLATFORM_SRC}
)

target_link_libraries(
  mirclientplatformandroid

  ${LIBHARDWARE_LIBRARIES}
)

add_library(
  mirclientplatformandroid-static STATIC

  ${ANDROID_PLATFORM_SRC}
)

set_target_properties(
  mirclientplatformandroid-static PROPERTIES
  OUTPUT_NAME mirclientplatform-static
  ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}/android
)

target_link_libraries(
  mirclientplatformandroid-static
  mirclient

  ${LIBHARDWARE_LIBRARIES}
)


install(TARGETS mirclientplatformandroid LIBRARY DESTINATION ${MIR_CLIENT_PLATFORM_PATH})

if (MIR_TEST_PLATFORM STREQUAL "android")
  add_custom_target(static-client-platform-symlink ALL
    DEPENDS mirclientplatformandroid-static
    COMMAND ${CMAKE_COMMAND} -E remove libmirclientplatform-static.a
    COMMAND ${CMAKE_COMMAND} -E create_symlink android/$<TARGET_FILE_NAME:mirclientplatformandroid-static> libmirclientplatform-static.a
    WORKING_DIRECTORY ${LIBRARY_OUTPUT_PATH}
  )
endif()
