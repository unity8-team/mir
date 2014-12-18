add_library(mirplatforminputevdevobjects OBJECT
    android_device_provider.cpp
    evdev_device_detection.cpp
    evdev_input_device_factory.cpp
    libinput_device_provider.cpp
    )

add_library(mirplatforminputevdev SHARED
  platform.cpp
  $<TARGET_OBJECTS:mirplatforminputevdevobjects>
)

set_target_properties(
  mirplatforminputevdev PROPERTIES
  OUTPUT_NAME input-evdev
  LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}/server-modules
  PREFIX ""
  LINK_FLAGS "-Wl,--exclude-libs=ALL -Wl,--version-script,${input_platform_symbol_map}"
)

target_link_libraries(mirplatforminputevdev
  mirplatform
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
)
