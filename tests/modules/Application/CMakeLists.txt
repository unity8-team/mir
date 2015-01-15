set(
  APPLICATION_TEST_SOURCES
  application_test.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(application_test ${APPLICATION_TEST_SOURCES})

target_link_libraries(
  application_test

  unityapplicationplugin
  qpa-mirserver

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARIES}
)

add_test(Application, application_test)
