include(CMakeDependentOption)

add_definitions(-DTEST_RECORDINGS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/input_recordings/")

include_directories(${DRM_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS} ${UMOCKDEV_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
# TODO: Aim to remove
include_directories(${MIR_XCURSOR_INCLUDE_DIRECTORIES})

set(
  UNIT_TEST_SOURCES

  test_gmock_fixes.cpp
  test_recursive_read_write_mutex.cpp
  test_asio_main_loop.cpp
  shared_library_test.cpp
  test_raii.cpp
  test_udev_wrapper.cpp
  test_variable_length_array.cpp
  test_thread_name.cpp
  test_default_emergency_cleanup.cpp
  test_basic_observers.cpp
  test_shared_library_prober.cpp
  test_fd.cpp
)

add_subdirectory(options/)
add_subdirectory(client/)
add_subdirectory(compositor/)
add_subdirectory(frontend/)
add_subdirectory(logging/)
add_subdirectory(shell/)
add_subdirectory(geometry/)
add_subdirectory(graphics/)
add_subdirectory(input/)
add_subdirectory(android_input/)
add_subdirectory(scene/)
add_subdirectory(draw/)

link_directories(${LIBRARY_OUTPUT_PATH})

add_executable(mir_unit_tests 
  ${UNIT_TEST_SOURCES}
  $<TARGET_OBJECTS:mirclientobjects>
)

uses_android_input(mir_unit_tests)

target_link_libraries(
  mir_unit_tests

  mirserver
  mirdraw
  mirtestdraw

  mir-test
  mir-test-doubles
  mir-test-doubles-platform
  mir-test-framework

  3rd_party

  ${PROTOBUF_LIBRARIES}
  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARY}
  ${GMOCK_MAIN_LIBRARY}
  ${Boost_LIBRARIES}
  ${UMOCKDEV_LIBRARIES}
  # Mesa platform dependencies
  ${DRM_LDFLAGS} ${DRM_LIBRARIES}
  ${GBM_LDFLAGS} ${GBM_LIBRARIES}
  # Android platform dependencies
  ${LIBHARDWARE_LIBRARIES}
  ${ANDROID_PROPERTIES_LIBRARIES}
  # Shared platform dependencies
  ${EGL_LDFLAGS} ${EGL_LIBRARIES}
  ${GLESv2_LDFLAGS} ${GLESv2_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT} # Link in pthread.
)

# Umockdev uses glib, which uses the deprecated "register" allocation specifier
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dregister=")

CMAKE_DEPENDENT_OPTION(
  MIR_RUN_UNIT_TESTS
  "Run unit tests as part of default testing"
  ON
  "MIR_BUILD_UNIT_TESTS"
  OFF)

if (MIR_TEST_PLATFORM STREQUAL "mesa")
  set (MIR_CLIENT_PLATFORM mir-client-platform-mesa.so)
endif()

if (MIR_TEST_PLATFORM STREQUAL "android")
  set (MIR_CLIENT_PLATFORM mir-client-platform-android.so)
endif()

if (MIR_RUN_UNIT_TESTS)
  mir_discover_tests(mir_unit_tests
                       LD_PRELOAD=libumockdev-preload.so.0
                       G_SLICE=always-malloc
                       G_DEBUG=gc-friendly
                       MIR_CLIENT_PLATFORM_LIB=${LIBRARY_OUTPUT_PATH}/${MIR_CLIENT_PLATFORM})
endif (MIR_RUN_UNIT_TESTS)

install(
    TARGETS mir_unit_tests
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET mir_unit_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/test_data ${CMAKE_BINARY_DIR}/bin/test_data
  COMMENT "Copying test data to build dir..."
)
