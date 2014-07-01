include_directories(
    ${CMAKE_BINARY_DIR}/include
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC -Wl,--no-as-needed -pthread")

add_executable(test_android_ubuntu_app_api test_c_api.cpp)
target_link_libraries(test_android_ubuntu_app_api ubuntu_application_api EGL GLESv2)

add_executable(test_android_ubuntu_app_api_multiple_surfaces test_multiple_surfaces_event_delivery.cpp)
target_link_libraries(test_android_ubuntu_app_api_multiple_surfaces ubuntu_application_api EGL GLESv2)

add_executable(test_android_sensors_api test_sensors_api.cpp)
target_link_libraries(test_android_sensors_api ubuntu_application_api)

add_executable(test_ubuntu_app_api_location_service test_location_api.cpp)
target_link_libraries(test_ubuntu_app_api_location_service ubuntu_application_api)

add_executable(test_ubuntu_haptic_device test_haptic_device.cpp)
target_link_libraries(test_ubuntu_haptic_device ubuntu_application_api)

install(TARGETS
    test_android_ubuntu_app_api
    test_android_ubuntu_app_api_multiple_surfaces
    test_android_sensors_api
    test_ubuntu_app_api_location_service
    test_ubuntu_haptic_device
    DESTINATION bin
)
