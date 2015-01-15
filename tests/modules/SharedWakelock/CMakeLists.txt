set(
  SHARED_WAKELOCK_TEST_SOURCES
  sharedwakelock_test.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/tests/modules/common
)

add_executable(sharedwakelock_test ${SHARED_WAKELOCK_TEST_SOURCES})

target_link_libraries(
  sharedwakelock_test

  unityapplicationplugin

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARIES}
)

add_test(SharedWakelock, sharedwakelock_test)
