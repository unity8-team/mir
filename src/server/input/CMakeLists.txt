# For the benefit of android/...
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(
  INPUT_SOURCES

  event_filter_chain.cpp

  null_input_configuration.cpp
  null_input_report.cpp
  nested_input_configuration.cpp
  nested_input_relay.cpp
  display_input_region.cpp
  vt_filter.cpp
)

add_subdirectory(android)

add_library(
  mirinput STATIC

  ${INPUT_SOURCES}
)
uses_android_input(mirinput)
