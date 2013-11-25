add_executable(
  symbol-resolution-tests

  test_client_symbol_resolution.cpp
)


target_link_libraries(
  symbol-resolution-tests

  ${GTEST_BOTH_LIBRARIES}
  -ldl
)

set_target_properties(
  symbol-resolution-tests

  PROPERTIES
  COMPILE_DEFINITIONS MIRCLIENT_LIBRARY_FILE=\"$<TARGET_SONAME_FILE:mirclient>\"
)

add_dependencies(symbol-resolution-tests mirclient)

mir_discover_tests(symbol-resolution-tests)
