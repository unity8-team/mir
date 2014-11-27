set(
  TASK_CONTROLLER_TEST_SOURCES
  taskcontroller_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(taskcontroller_test ${TASK_CONTROLLER_TEST_SOURCES})
add_dependencies(taskcontroller_test GMock)

target_link_libraries(
  taskcontroller_test

  qpa-mirserver
  unityapplicationplugin

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_BOTH_LIBRARIES}
)

add_test(TaskController, taskcontroller_test)
