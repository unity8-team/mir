set(
  CLIPBOARD_TEST_SOURCES
  clipboard_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(ClipboardTest ${CLIPBOARD_TEST_SOURCES})

target_link_libraries(
  ClipboardTest
  qpa-mirserver

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARIES}
)

add_test(Clipboard, ClipboardTest)
