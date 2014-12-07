list(APPEND UNIT_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test_android_device_provider.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_evdev_device_detection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_evdev_input_device_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_libinput_device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_libinput_device_provider.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_libinput_wrapper.cpp
  $<TARGET_OBJECTS:mirplatforminputevdevobjects>
)

set(
  UNIT_TEST_SOURCES
  ${UNIT_TEST_SOURCES}
  PARENT_SCOPE)
