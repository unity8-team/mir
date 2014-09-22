find_package(Qt5Gui)

set(
  EVENT_FEEDER_TEST_SOURCES
  qteventfeeder_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${CMAKE_SOURCE_DIR}/src/common
  ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(QtEventFeederTest ${EVENT_FEEDER_TEST_SOURCES})

target_link_libraries(
  QtEventFeederTest
  qpa-mirserver
  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_BOTH_LIBRARIES}
)

add_test(QtEventFeeder, QtEventFeederTest)