set(
  SESSION_MANAGER_TEST_SOURCES
  session_manager_test.cpp
  session_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(sessionmanager_test ${SESSION_MANAGER_TEST_SOURCES})

target_link_libraries(
  sessionmanager_test

  qpa-mirserver
  unityapplicationplugin

  Qt5::Test

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARIES}
)

add_test(SessionManager, sessionmanager_test)
