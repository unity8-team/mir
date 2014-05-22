set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

include_directories(
  ${CMAKE_BINARY_DIR}/include
)

add_executable(test_android_gps_api test_gps_api.cpp)
target_link_libraries(test_android_gps_api ubuntu_platform_hardware_api)

add_executable(test_hardware_alarms_api test_alarms_api.cpp)
target_link_libraries(test_hardware_alarms_api ubuntu_platform_hardware_api)

install(TARGETS
	test_android_gps_api
    test_hardware_alarms_api
	DESTINATION bin
)
