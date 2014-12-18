set(
  GENERAL_TEST_SOURCES
  objectlistmodel_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${CMAKE_SOURCE_DIR}/src/common
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(general_test ${GENERAL_TEST_SOURCES})

target_link_libraries(
  general_test

  qpa-mirserver
  unityapplicationplugin

  Qt5::Gui

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARIES}
)

add_test(General general_test)
