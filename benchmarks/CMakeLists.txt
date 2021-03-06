add_custom_target(benchmarks)   # XXX Is this used by anything?

add_subdirectory(cpu)
add_subdirectory(memory)
add_dependencies(benchmarks cpu_benchmarks memory_benchmarks)

if (MIR_ENABLE_TESTS)
  # Shouldn't tests dependent things be in tests/?
  add_subdirectory(frame-uniformity)
  add_dependencies(benchmarks frame_uniformity_test_client)
endif ()

add_executable(benchmark_multiplexing_dispatchable
  benchmark_multiplexing_dispatchable.cpp
)

target_link_libraries(benchmark_multiplexing_dispatchable
  mircommon
)

# Note: We need to write \$ENV{DESTDIR} (note the \$) to make
# CMake replace the DESTDIR variable at installation time rather
# than configuration time
install(CODE "execute_process(COMMAND python3 mir_perf_framework_setup.py install -f --prefix=${CMAKE_INSTALL_PREFIX} --root=\$ENV{DESTDIR} --install-layout=deb --no-compile WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})")

set(MIR_PERF_SCRIPTS
  key_event_latency.py
  nested_client_to_display_buffer_latency.py
  touch_event_latency.py
)

install(
  FILES ${MIR_PERF_SCRIPTS}
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mir-perf-framework
)
