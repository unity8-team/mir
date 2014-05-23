find_package(PkgConfig REQUIRED)
pkg_check_modules(PROCESS_CPP process-cpp REQUIRED)

include_directories(${GTEST_ROOT}/src)
include_directories(${PROCESS_CPP_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

add_executable(
    test_ua_sensors_real
    test_ua_sensors_real.cpp
)

add_executable(
    test_ua_sensors_mock
    test_ua_sensors_mock.cpp
)

target_link_libraries(
    test_ua_sensors_mock

    ubuntu_application_api
    gtest
    gtest_main
    ${PROCESS_CPP_LIBRARIES}
)

target_link_libraries(
    test_ua_sensors_real

    ubuntu_application_api
    gtest
    gtest_main
    ${PROCESS_CPP_LIBRARIES}
)

add_test(
    test_ua_sensors_mock
    
    env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src/ubuntu:${CMAKE_BINARY_DIR}/src/ubuntu/application/testbackend ${CMAKE_CURRENT_BINARY_DIR}/test_ua_sensors_mock
)

if(DEFINED ENV{UBUNTU_PLATFORM_API_BACKEND})
    add_test(
        test_ua_sensors_real

        env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src/ubuntu:${CMAKE_BINARY_DIR}/src/ubuntu/application/touch:${CMAKE_BINARY_DIR}/src/ubuntu/desktop ${CMAKE_CURRENT_BINARY_DIR}/test_ua_sensors_real
    )
endif()
