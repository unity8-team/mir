set(
  APPLICATION_MANAGER_TEST_SOURCES
  application_manager_test.cpp
  ${CMAKE_SOURCE_DIR}/src/common/debughelpers.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
  ${CMAKE_SOURCE_DIR}/src/modules
  ${CMAKE_SOURCE_DIR}/tests/modules/common
  ${MIRSERVER_INCLUDE_DIRS}
)

add_executable(applicationmanager_test ${APPLICATION_MANAGER_TEST_SOURCES})
add_dependencies(applicationmanager_test GMock)

target_link_libraries(
  applicationmanager_test
  
  qpa-mirserver
  unityapplicationplugin

  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_BOTH_LIBRARIES}
)

qt5_use_modules(applicationmanager_test Test)

add_test(ApplicationManager applicationmanager_test)
