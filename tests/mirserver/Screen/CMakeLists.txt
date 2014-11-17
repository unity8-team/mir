set(
  SCREEN_TEST_SOURCES
  screen_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${CMAKE_SOURCE_DIR}/src/common
  ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(ScreenTest ${SCREEN_TEST_SOURCES})

target_link_libraries(
  ScreenTest
  qpa-mirserver

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_BOTH_LIBRARIES}
)

add_test(Screen, ScreenTest)
