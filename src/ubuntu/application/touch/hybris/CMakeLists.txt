set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

add_library(
  ubuntu_application_api_hybris STATIC
  
  ubuntu_application_sensors_hybris.cpp
)

include_directories(
  ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(
  ubuntu_application_api_hybris
  
  ${Hybris}
)
