pkg_check_modules(UMOCKDEV REQUIRED umockdev-1.0>=0.6)
if (NOT UMOCKDEV_FOUND)
  message(FATAL_ERROR "Umockdev not found, cannot build without disabling tests (via MIR_ENABLE_TESTS).")
endif()

include_directories(${MIR_3RD_PARTY_INCLUDE_DIRECTORIES})
include_directories(${MIR_ANDROID_INCLUDE_DIRECTORIES})

if ("${CMAKE_CXX_COMPILER}" MATCHES "clang")
  # Avoid clang complaints about poor quality gmock/gtest headers
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=null-dereference")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=overloaded-virtual")
endif()

if (MIR_TEST_PLATFORM STREQUAL "android")
  #avoid complaints about poor quality android headers
  include_directories(SYSTEM ${LIBHARDWARE_INCLUDE_DIRS})
  add_definitions(-DANDROID)
endif()

include_directories(
  ${PROJECT_SOURCE_DIR}/include/platform
  ${PROJECT_SOURCE_DIR}/include/server
  ${PROJECT_SOURCE_DIR}/include/client
  ${PROJECT_SOURCE_DIR}/include/test)

option(MIR_BUILD_ACCEPTANCE_TESTS "Build acceptance tests" ON)
option(MIR_BUILD_INTEGRATION_TESTS "Build integration tests" ON)
option(MIR_BUILD_PERFORMANCE_TESTS "Build performance tests" ON)
option(MIR_BUILD_UNIT_TESTS "Build unit tests" ON)

if (MIR_BUILD_ACCEPTANCE_TESTS)
  add_subdirectory(acceptance-tests/)
endif (MIR_BUILD_ACCEPTANCE_TESTS)

if (MIR_BUILD_INTEGRATION_TESTS)
  add_subdirectory(integration-tests/)
endif (MIR_BUILD_INTEGRATION_TESTS)

if (MIR_BUILD_PERFORMANCE_TESTS)
    add_subdirectory(performance-tests/)
endif(MIR_BUILD_PERFORMANCE_TESTS)

if (MIR_BUILD_UNIT_TESTS)
  add_subdirectory(unit-tests/)
endif (MIR_BUILD_UNIT_TESTS)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MIR_ANDROID_INPUT_COMPILE_FLAGS}")

add_subdirectory(mir_test/)
add_subdirectory(mir_test_framework/)
add_subdirectory(mir_test_doubles/)
add_subdirectory(client-language/)
add_subdirectory(mir-stress/)

add_library(mirtesting SHARED
  $<TARGET_OBJECTS:mir-test>
  $<TARGET_OBJECTS:mir-test-framework>
  $<TARGET_OBJECTS:mir-test-doubles>
  $<TARGET_OBJECTS:mir-test-doubles-platform>
  $<TARGET_OBJECTS:mirclientobjects>  # XXX redundant but required
  $<TARGET_OBJECTS:mirplatformgraphicsmesaobjects>  # XXX redundant but required
)

target_link_libraries(mirtesting
  mirserver
  mirclient
  3rd_party
  -ldrm  #FIXME: Mesa only
  ${PROTOBUF_LIBRARIES}
  ${Boost_LIBRARIES}
  ${GTEST_LIBRARY}
  ${GMOCK_LIBRARY}
  ${GMOCK_MAIN_LIBRARY}
  ${UMOCKDEV_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT} # Link in pthread.
)

uses_android_input(mirtesting)

mir_add_memcheck_test()
