set(
  MIR_SURFACE_ITEM_TEST_SOURCES
  mirsurfaceitem_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/modules/Unity/Application
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(mirsurfaceitem_test ${MIR_SURFACE_ITEM_TEST_SOURCES})

target_link_libraries(
  mirsurfaceitem_test

  qpa-mirserver
  unityapplicationplugin

  Qt5::Test

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARIES}
)

add_test(MirSurfaceItem mirsurfaceitem_test)
