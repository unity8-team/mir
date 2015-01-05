add_definitions(-DTEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

set(
  DESKTOP_FILE_READER_TEST_SOURCES
  desktopfilereader_test.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(desktop_file_reader_test ${DESKTOP_FILE_READER_TEST_SOURCES})

target_link_libraries(
  desktop_file_reader_test
  
  qpa-mirserver
  unityapplicationplugin

  Qt5::Gui

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARIES}
)

add_test(DesktopFileReader desktop_file_reader_test)
