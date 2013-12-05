include_directories(${GTEST_ROOT}/src)

add_executable(
    test_ubuntu_application_sensors
    test_ubuntu_application_sensors.cpp
)

target_link_libraries(
    test_ubuntu_application_sensors

    ubuntu_application_api
    gtest
    gtest_main
)

# we need to call the tests with pointing to the locally built test platform
# library
add_test(
    test_ubuntu_application_sensors
    
    env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src/ubuntu/testbackend ${CMAKE_CURRENT_BINARY_DIR}/test_ubuntu_application_sensors
)

