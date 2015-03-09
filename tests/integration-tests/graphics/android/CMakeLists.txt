list(APPEND INTEGRATION_TESTS_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/test_client_render.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patterns.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/android_graphics_region_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_buffer_integration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_display_integration.cpp
)
set(INTEGRATION_TESTS_SRCS ${INTEGRATION_TESTS_SRCS} PARENT_SCOPE)

list(APPEND INTEGRATION_TESTS_LINKS
  mirplatformgraphicsandroid-private
)
set(INTEGRATION_TESTS_LINKS ${INTEGRATION_TESTS_LINKS} PARENT_SCOPE)
