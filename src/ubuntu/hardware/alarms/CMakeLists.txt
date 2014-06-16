set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -fPIC")

include_directories(/usr/include/android)

add_library(
  ubuntu_hardware_alarm

  alarm.cpp
)