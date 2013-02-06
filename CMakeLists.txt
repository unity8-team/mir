cmake_minimum_required(VERSION 2.6)

project(ubuntu_platform_api)

set(BIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/bin)
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE})

set(INSTALL_TARGETS_DEFAULT_ARGS RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
                                 LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
                                 ARCHIVE DESTINATION "${LIB_INSTALL_DIR}")

include_directories(include/)

add_subdirectory(src/)
add_subdirectory(include/)
