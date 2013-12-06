find_package(PkgConfig REQUIRED)
pkg_check_modules(PROCESS_CPP process-cpp REQUIRED)

include_directories(${GTEST_ROOT}/src)
include_directories(${PROCESS_CPP_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

add_executable(
    test_ubuntu_application_sensors
    test_ubuntu_application_sensors.cpp
)

target_link_libraries(
    test_ubuntu_application_sensors

    ubuntu_application_api
    gtest
    gtest_main
    ${PROCESS_CPP_LIBRARIES}
)

# we need to call the tests with pointing to the locally built test platform
# library
add_test(
    test_ubuntu_application_sensors
    
    env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src/ubuntu/testbackend ${CMAKE_CURRENT_BINARY_DIR}/test_ubuntu_application_sensors
)

