include_directories(
    ${CMAKE_SOURCE_DIR}/include/client
    ${CMAKE_SOURCE_DIR}/include/common
    )
add_library(mirplatforminput OBJECT
    platform.cpp
    input_device.cpp
    event_builder.cpp
    )
