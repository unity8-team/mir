cmake_minimum_required (VERSION 2.6)
# Create target to discover tests
include (CMakeParseArguments)

include(CMakeDependentOption)
file(REMOVE ${CMAKE_BINARY_DIR}/discover_all_tests.sh)

option(
  ENABLE_MEMCHECK_OPTION
  "If set to ON, enables automatic creation of memcheck targets"
  OFF
)

option(
  MIR_USE_PRECOMPILED_HEADERS
  "Use precompiled headers"
  ON
)

if(ENABLE_MEMCHECK_OPTION)
  find_program(
    VALGRIND_EXECUTABLE
    valgrind)

  if(VALGRIND_EXECUTABLE)
    set(VALGRIND_CMD "${VALGRIND_EXECUTABLE}" "--error-exitcode=1" "--trace-children=yes" "--leak-check=full" "--show-leak-kinds=definite" "--errors-for-leak-kinds=definite")
    set(VALGRIND_CMD ${VALGRIND_CMD} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_generic")
    set(VALGRIND_CMD ${VALGRIND_CMD} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_glibc_2.21")
    if (TARGET_ARCH STREQUAL "arm-linux-gnueabihf")
      set(VALGRIND_CMD ${VALGRIND_CMD} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_armhf")
    endif()
  else(VALGRIND_EXECUTABLE)
    message("Not enabling memcheck as valgrind is missing on your system")
  endif(VALGRIND_EXECUTABLE)
endif(ENABLE_MEMCHECK_OPTION)

function (list_to_string LIST_VAR PREFIX STR_VAR)
  foreach (value ${LIST_VAR})
    set(tmp_str "${tmp_str} ${PREFIX} ${value}")
  endforeach()
  set(${STR_VAR} "${tmp_str}" PARENT_SCOPE)
endfunction()

function (mir_discover_tests EXECUTABLE)
  # Set vars
  set(test_cmd_no_memcheck "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXECUTABLE}")
  set(test_cmd "${test_cmd_no_memcheck}")
  set(test_env ${ARGN})
  set(test_name ${EXECUTABLE})
  set(test_no_memcheck_filter)
  set(test_exclusion_filter)

  if(ENABLE_MEMCHECK_OPTION)
    set(test_cmd ${VALGRIND_CMD} ${test_cmd_no_memcheck})
    set(test_no_memcheck_filter "*DeathTest.*")
  endif()

  if(cmake_build_type_lower MATCHES "threadsanitizer")
    find_program(LLVM_SYMBOLIZER llvm-symbolizer-3.6)
    if (LLVM_SYMBOLIZER)
        set(TSAN_EXTRA_OPTIONS "external_symbolizer_path=${LLVM_SYMBOLIZER}")
    endif()
    # Space after ${TSAN_EXTRA_OPTIONS} works around bug in TSAN env. variable parsing 
    list(APPEND test_env "TSAN_OPTIONS=\"suppressions=${CMAKE_SOURCE_DIR}/tools/tsan-suppressions second_deadlock_stack=1 halt_on_error=1 history_size=7 ${TSAN_EXTRA_OPTIONS} \"")
    # TSan does not support multi-threaded fork
    # TSan may open fds so "surface_creation_does_not_leak_fds" will not work as written
    # TSan deadlocks when running StreamTransportTest/0.SendsFullMessagesWhenInterrupted - disable it until understood
    set(test_exclusion_filter "UnresponsiveClient.does_not_hang_server:DemoInProcessServerWithStubClientPlatform.surface_creation_does_not_leak_fds:StreamTransportTest/0.SendsFullMessagesWhenInterrupted")
  endif()

  # Final commands
  set(test_cmd "${test_cmd}" "--gtest_filter=-${test_no_memcheck_filter}:${test_exclusion_filter}")
  set(test_cmd_no_memcheck "${test_cmd_no_memcheck}" "--gtest_filter=${test_no_memcheck_filter}:-${test_exclusion_filter}")

  # Normal
  add_test(${test_name} ${test_cmd})
  set_property(TEST ${test_name} PROPERTY ENVIRONMENT ${test_env})
  if (test_no_memcheck_filter)
    add_test(${test_name}_no_memcheck ${test_cmd_no_memcheck})
    set_property(TEST ${test_name}_no_memcheck PROPERTY ENVIRONMENT ${test_env})
  endif()

  # ptest
  list_to_string("${test_env}" "--env" discover_env)
  list_to_string("${test_cmd}" "" discover_cmd)
  list_to_string("${test_cmd_no_memcheck}" "" discover_cmd_no_memcheck)

  file(APPEND ${CMAKE_BINARY_DIR}/discover_all_tests.sh
    "sh ${CMAKE_SOURCE_DIR}/tools/discover_gtests.sh ${discover_env} -- ${discover_cmd}\n")
  if (test_no_memcheck_filter)
    file(APPEND ${CMAKE_BINARY_DIR}/discover_all_tests.sh
      "sh ${CMAKE_SOURCE_DIR}/tools/discover_gtests.sh ${discover_env} -- ${discover_cmd_no_memcheck}\n")
  endif()
endfunction ()

function (mir_add_memcheck_test)
  if (ENABLE_MEMCHECK_OPTION)
    add_custom_target(memcheck_test ALL)
    mir_add_test(NAME "memcheck-test"
      COMMAND ${CMAKE_BINARY_DIR}/mir_gtest/fail_on_success.sh ${VALGRIND_CMD} ${CMAKE_BINARY_DIR}/mir_gtest/mir_test_memory_error)
    add_dependencies(memcheck_test mir_test_memory_error)
  endif()
endfunction()

function (mir_precompiled_header TARGET HEADER)
  if (MIR_USE_PRECOMPILED_HEADERS)
    get_property(TARGET_COMPILE_FLAGS TARGET ${TARGET} PROPERTY COMPILE_FLAGS)
    get_property(TARGET_INCLUDE_DIRECTORIES TARGET ${TARGET} PROPERTY INCLUDE_DIRECTORIES)
    foreach(dir ${TARGET_INCLUDE_DIRECTORIES})
      if (${dir} MATCHES "usr/include")
        set(TARGET_INCLUDE_DIRECTORIES_STRING "${TARGET_INCLUDE_DIRECTORIES_STRING} -isystem ${dir}")
      else()
        set(TARGET_INCLUDE_DIRECTORIES_STRING "${TARGET_INCLUDE_DIRECTORIES_STRING} -I${dir}")
      endif()
    endforeach()

    separate_arguments(
      PCH_CXX_FLAGS UNIX_COMMAND
      "${CMAKE_CXX_FLAGS} ${TARGET_COMPILE_FLAGS} ${TARGET_INCLUDE_DIRECTORIES_STRING}"
    )

    add_custom_command(
      OUTPUT ${TARGET}_precompiled.hpp.gch
      DEPENDS ${HEADER}
      COMMAND ${CMAKE_CXX_COMPILER} ${PCH_CXX_FLAGS} -x c++-header -c ${HEADER} -o ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_precompiled.hpp.gch
    )

    set_property(TARGET ${TARGET} APPEND_STRING PROPERTY COMPILE_FLAGS " -include ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_precompiled.hpp -Winvalid-pch ")

    add_custom_target(${TARGET}_pch DEPENDS ${TARGET}_precompiled.hpp.gch)
    add_dependencies(${TARGET} ${TARGET}_pch)
  endif()
endfunction()

function (mir_add_wrapped_executable TARGET)
  set(REAL_EXECUTABLE .${TARGET}-uninstalled)

  list(GET ARGN 0 modifier)
  if ("${modifier}" STREQUAL "NOINSTALL")
    list(REMOVE_AT ARGN 0)
  else()
    install(PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${REAL_EXECUTABLE}
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      RENAME ${TARGET}
    )
  endif()

  add_executable(${TARGET} ${ARGN})
  set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME ${REAL_EXECUTABLE}
    SKIP_BUILD_RPATH TRUE
  )

  add_custom_target(${TARGET}-wrapped
    ln -fs wrapper ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}
  )
  add_dependencies(${TARGET} ${TARGET}-wrapped)
endfunction()

function (mir_add_test)
  # Add test normally
  add_test(${ARGN})

  # Add to to discovery for parallel test running
  set(one_value_args "NAME" WORKING_DIRECTORY)
  set(multi_value_args "COMMAND")
  cmake_parse_arguments(MAT "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  foreach (cmd ${MAT_COMMAND})
    set(cmdstr "${cmdstr} \\\"${cmd}\\\"")
  endforeach()

  file(APPEND ${CMAKE_BINARY_DIR}/discover_all_tests.sh
    "echo \"add_test(${MAT_NAME} ${cmdstr})\"\n")

  if (MAT_WORKING_DIRECTORY)
    file(APPEND ${CMAKE_BINARY_DIR}/discover_all_tests.sh
      "echo \"set_tests_properties(${MAT_NAME} PROPERTIES WORKING_DIRECTORY \\\"${MAT_WORKING_DIRECTORY}\\\")\"\n")
  endif()
endfunction()
