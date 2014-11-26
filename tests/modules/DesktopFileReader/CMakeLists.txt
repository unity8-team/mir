find_package(Qt5Gui)
find_package(Qt5Test)

add_definitions(-DTEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

set(
  DESKTOP_FILE_READER_TEST_SOURCES
  desktopfilereader_test.cpp
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

add_executable(desktop_file_reader_test ${DESKTOP_FILE_READER_TEST_SOURCES})
add_dependencies(desktop_file_reader_test GMock)

target_link_libraries(
  desktop_file_reader_test
  
  Qt5::Test

  qpa-mirserver
  unityapplicationplugin

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_BOTH_LIBRARIES}
)

add_test(DesktopFileReader desktop_file_reader_test)