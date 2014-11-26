find_package(Qt5Gui)
find_package(Qt5Test)

set(
  SESSION_MANAGER_TEST_SOURCES
  session_manager_test.cpp
  session_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${CMAKE_SOURCE_DIR}/src/common
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
  ${Qt5Test_INCLUDE_DIRS}
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(sessionmanager_test ${SESSION_MANAGER_TEST_SOURCES})
add_dependencies(sessionmanager_test GMock)

target_link_libraries(
  sessionmanager_test

  qpa-mirserver
  unityapplicationplugin

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_BOTH_LIBRARIES}
)

add_test(SessionManager, sessionmanager_test)