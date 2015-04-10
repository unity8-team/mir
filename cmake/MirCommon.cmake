cmake_minimum_required (VERSION 2.6)
# Create target to discover tests

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
    set(VALGRIND_ARGS "--error-exitcode=1" "--trace-children=yes" "--leak-check=full" "--show-leak-kinds=definite" "--errors-for-leak-kinds=definite")
    set(VALGRIND_ARGS ${VALGRIND_ARGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_generic")
    set(VALGRIND_ARGS ${VALGRIND_ARGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_glibc_2.21")
    set(DISCOVER_FLAGS "--enable-memcheck")
    set(DISCOVER_FLAGS ${DISCOVER_FLAGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_generic")
    set(DISCOVER_FLAGS ${DISCOVER_FLAGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_glibc_2.21")
    if (TARGET_ARCH STREQUAL "arm-linux-gnueabihf")
        set(VALGRIND_ARGS ${VALGRIND_ARGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_armhf")
        set(DISCOVER_FLAGS ${DISCOVER_FLAGS} "--suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions_armhf")
    endif()
  else(VALGRIND_EXECUTABLE)
    message("Not enabling memcheck as valgrind is missing on your system")
  endif(VALGRIND_EXECUTABLE)
endif(ENABLE_MEMCHECK_OPTION)

function (mir_discover_tests EXECUTABLE)
  execute_process(
    COMMAND uname -r
    OUTPUT_VARIABLE KERNEL_VERSION_FULL
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(REGEX MATCH "^[0-9]+[.][0-9]+" KERNEL_VERSION ${KERNEL_VERSION_FULL})
  message(STATUS "Kernel version detected: " ${KERNEL_VERSION})
  # Some tests expect kernel version 3.11 and up
  if (${KERNEL_VERSION} VERSION_LESS "3.11")
    set(EXCLUDED_TESTS "AnonymousShmFile.*:MesaBufferAllocatorTest.software_buffers_dont_bypass:MesaBufferAllocatorTest.creates_software_rendering_buffer")
  endif()

  if(cmake_build_type_lower MATCHES "threadsanitizer")
    find_program(LLVM_SYMBOLIZER llvm-symbolizer-3.6)
    if (LLVM_SYMBOLIZER)
      set(TSAN_EXTRA_OPTIONS "external_symbolizer_path=${LLVM_SYMBOLIZER}")
    endif()
    list(APPEND ARGN "--add-environment" "TSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tools/tsan-suppressions second_deadlock_stack=1 halt_on_error=1 history_size=7 ${TSAN_EXTRA_OPTIONS}")
    # TSan does not support multi-threaded fork
    # TSan may open fds so "surface_creation_does_not_leak_fds" will not work as written
    # TSan deadlocks when running StreamTransportTest/0.SendsFullMessagesWhenInterrupted - disable it until understood
    set(EXCLUDED_TESTS "${EXCLUDED_TESTS}:UnresponsiveClient.does_not_hang_server:DemoInProcessServerWithStubClientPlatform.surface_creation_does_not_leak_fds:StreamTransportTest/0.SendsFullMessagesWhenIterrupted")
  endif()

  set(MAYBE_MEMCHECKED_EXECUTABLE ${EXECUTABLE_OUTPUT_PATH}/${EXECUTABLE})
  if (ENABLE_MEMCHECK_OPTION)
    set(MAYBE_MEMCHEKCED_EXECUTABLE ${VALGRIND_EXECUTABLE} ${VALGRIND_ARGS} ${MAYBE_MEMCHECKED_EXECUTABLE})
  endif()
  
  add_test(${EXECUTABLE} ${MAYBE_MEMCHECKED_EXECUTABLE} "--gtest_filter=-*DeathTest.*:${EXCLUDED_TESTS}")
  add_test(${EXECUTABLE}_death_tests ${EXECUTABLE_OUTPUT_PATH}/${EXECUTABLE} "--gtest_filter=*DeathTest.*:-${EXCLUDED_TESTS}")

  set_property(TEST ${EXECUTABLE} PROPERTY ENVIRONMENT ${ARGN})
  set_property(TEST ${EXECUTABLE}_death_tests PROPERTY ENVIRONMENT ${ARGN})
endfunction ()

function (mir_add_memcheck_test)
  if (ENABLE_MEMCHECK_OPTION)
    add_custom_target(
      memcheck_test ALL
    )
    ADD_TEST("memcheck-test" ${CMAKE_BINARY_DIR}/mir_gtest/fail_on_success.sh ${VALGRIND_EXECUTABLE} ${VALGRIND_ARGS} ${CMAKE_BINARY_DIR}/mir_gtest/mir_test_memory_error)
    add_dependencies(
      memcheck_test

      mir_test_memory_error
    )
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
    install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/${REAL_EXECUTABLE}
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
    ln -fs wrapper ${CMAKE_BINARY_DIR}/bin/${TARGET}
  )
  add_dependencies(${TARGET} ${TARGET}-wrapped)
endfunction()
