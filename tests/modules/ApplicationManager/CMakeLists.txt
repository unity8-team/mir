find_package(Qt5Gui)
find_package(Qt5Test)

set(
  APPLICATION_MANAGER_TEST_SOURCES
  application_manager_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
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

add_executable(applicationmanager_test ${APPLICATION_MANAGER_TEST_SOURCES})

target_link_libraries(
  applicationmanager_test
  
  Qt5::Test

  qpa-mirserver
  unityapplicationplugin

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_BOTH_LIBRARIES}
)