set(
  OPTIONS_SOURCES

  program_option.cpp
  default_configuration.cpp
)

add_library(
  miroptions STATIC

  ${OPTIONS_SOURCES}
)

target_link_libraries(
  miroptions

  ${Boost_PROGRAM_OPTIONS_LIBRARY}  
)
