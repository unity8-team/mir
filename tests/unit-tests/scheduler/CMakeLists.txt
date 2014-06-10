list(
  APPEND UNIT_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test_asio_main_loop.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_asio_server_action_queue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_asio_timer_service.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_synchronous_server_action.cpp
)

set(UNIT_TEST_SOURCES ${UNIT_TEST_SOURCES} PARENT_SCOPE)
