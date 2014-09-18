find_package(Qt5Gui)
find_package(Qt5Test)

set(
  MIR_SURFACE_ITEM_TEST_SOURCES
  mirsurfaceitem_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${CMAKE_SOURCE_DIR}/src/common
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/src/modules/Unity/Application
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
  ${Qt5Test_INCLUDE_DIRS}
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(mirsurfaceitem_test ${MIR_SURFACE_ITEM_TEST_SOURCES})

target_link_libraries(
  mirsurfaceitem_test

  qpa-mirserver
  unityapplicationplugin

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_BOTH_LIBRARIES}
)