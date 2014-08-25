cmake_minimum_required (VERSION 2.6)
# Create target to discover tests

include(CMakeDependentOption)

CMAKE_DEPENDENT_OPTION(
  DISABLE_GTEST_TEST_DISCOVERY
  "If set to ON, disables fancy test autodiscovery and switches back to classic add_test behavior"
  OFF
  "NOT CMAKE_CROSSCOMPILING"
  ON)

option(
  ENABLE_MEMCHECK_OPTION
  "If set to ON, enables automatic creation of memcheck targets"
  OFF
)

if(ENABLE_MEMCHECK_OPTION)
  find_program(
    VALGRIND_EXECUTABLE
    valgrind)

  if(VALGRIND_EXECUTABLE)
    set(VALGRIND_ARGS "--error-exitcode=1" "--trace-children=yes" "--leak-check=full" "--show-leak-kinds=definite" "--errors-for-leak-kinds=definite")
    set(VALGRIND_ARGS ${VALGRIND_ARGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_generic")
    set(DISCOVER_FLAGS "--enable-memcheck")
    set(DISCOVER_FLAGS ${DISCOVER_FLAGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_generic")
    if (TARGET_ARCH STREQUAL "arm-linux-gnueabihf")
        set(VALGRIND_ARGS ${VALGRIND_ARGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_armhf")
        set(DISCOVER_FLAGS ${DISCOVER_FLAGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_armhf")
    endif()
  else(VALGRIND_EXECUTABLE)
    message("Not enabling memcheck as valgrind is missing on your system")
  endif(VALGRIND_EXECUTABLE)
endif(ENABLE_MEMCHECK_OPTION)

function (mir_discover_tests EXECUTABLE)
  if(DISABLE_GTEST_TEST_DISCOVERY)
    add_test(${EXECUTABLE} ${VALGRIND_EXECUTABLE} ${VALGRIND_ARGS} ${EXECUTABLE_OUTPUT_PATH}/${EXECUTABLE} "--gtest_filter=-*DeathTest.*")
    add_test(${EXECUTABLE}_death_tests ${EXECUTABLE_OUTPUT_PATH}/${EXECUTABLE} "--gtest_filter=*DeathTest.*")
    if (${ARGC} GREATER 1)
      set_property(TEST ${EXECUTABLE} PROPERTY ENVIRONMENT ${ARGN})
      set_property(TEST ${EXECUTABLE}_death_tests PROPERTY ENVIRONMENT ${ARGN})
    endif()
  else()
    set(CHECK_TEST_DISCOVERY_TARGET_NAME "check_discover_tests_in_${EXECUTABLE}")
    set(TEST_DISCOVERY_TARGET_NAME "discover_tests_in_${EXECUTABLE}")
    message(STATUS "Defining targets: ${CHECK_TEST_DISCOVERY_TARGET_NAME} and ${TEST_DISCOVERY_TARGET_NAME}")

    # These targets are always considered out-of-date, and are always run (at least for normal builds, except for make test/install).
    add_custom_target(
      ${CHECK_TEST_DISCOVERY_TARGET_NAME} ALL
      ${EXECUTABLE_OUTPUT_PATH}/${EXECUTABLE} --gtest_list_tests > /dev/null
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Check that discovering Tests in ${EXECUTABLE} works")
      
    if (MIR_BUILD_PLATFORM_ANDROID)
      add_dependencies(${CHECK_TEST_DISCOVERY_TARGET_NAME} mirplatformgraphicsandroid)
    endif()
    
    if (MIR_BUILD_PLATFORM_MESA)
      add_dependencies(${CHECK_TEST_DISCOVERY_TARGET_NAME} mirplatformgraphicsmesa)
    endif()

    if (${ARGC} GREATER 1)
      foreach (env ${ARGN})
        list(APPEND EXTRA_ENV_FLAGS "--add-environment" "${env}")
      endforeach()
    endif()

    add_custom_target(
      ${TEST_DISCOVERY_TARGET_NAME} ALL
      ${EXECUTABLE_OUTPUT_PATH}/${EXECUTABLE} --gtest_list_tests | ${CMAKE_BINARY_DIR}/mir_gtest/mir_discover_gtest_tests --executable=${EXECUTABLE_OUTPUT_PATH}/${EXECUTABLE} ${DISCOVER_FLAGS}
      ${EXTRA_ENV_FLAGS}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Discovering Tests in ${EXECUTABLE}" VERBATIM)

    add_dependencies(
      ${CHECK_TEST_DISCOVERY_TARGET_NAME}
      ${EXECUTABLE})

    add_dependencies(
      ${TEST_DISCOVERY_TARGET_NAME}

      ${CHECK_TEST_DISCOVERY_TARGET_NAME}
      ${EXECUTABLE}
      mir_discover_gtest_tests)

  endif()
endfunction ()

function (mir_add_memcheck_test)
  if (ENABLE_MEMCHECK_OPTION)
      if(DISABLE_GTEST_TEST_DISCOVERY)
	add_custom_target(
	  memcheck_test ALL
	)
	ADD_TEST("memcheck-test" ${CMAKE_BINARY_DIR}/mir_gtest/fail_on_success.sh ${VALGRIND_EXECUTABLE} ${VALGRIND_ARGS} ${CMAKE_BINARY_DIR}/mir_gtest/mir_test_memory_error)
	add_dependencies(
	  memcheck_test

	  mir_test_memory_error
	)
      else()
        add_custom_target(
          memcheck_test ALL
          ${CMAKE_BINARY_DIR}/mir_gtest/mir_discover_gtest_tests --executable=${CMAKE_BINARY_DIR}/mir_gtest/mir_test_memory_error --memcheck-test ${DISCOVER_FLAGS}
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          COMMENT "Adding memcheck test" VERBATIM)

        add_dependencies(
          memcheck_test

          mir_discover_gtest_tests
          mir_test_memory_error)
      endif()
  endif()
endfunction()