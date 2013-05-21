cmake_minimum_required (VERSION 2.6)
project (kvant)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)


set(
  STARTUP_SOURCES
  eglapp.c
  startup_cost_mir.c
)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
include (FindMirClient.cmake)
include (FindGLESv2.cmake)
include (FindEGL.cmake)
find_package(MirClient REQUIRED)
find_package(EGL REQUIRED)
find_package(GLESv2 REQUIRED)

include_directories(${MirClient_INCLUDE_DIRS})
include_directories (${GLESv2_INCLUDE_DIRS})
include_directories (${EGL_INCLUDE_DIRS})

 
add_executable(startup-cost ${STARTUP_SOURCES})

target_link_libraries(
    startup-cost
    ${MirClient_LIBRARIES}
    ${EGL_LIBRARIES}
    ${GLESv2_LIBRARIES}
)
