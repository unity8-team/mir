cmake_minimum_required(VERSION 2.6)

project(ubuntu_platform_api)

set(BIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/bin)
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE})

set(INSTALL_TARGETS_DEFAULT_ARGS RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
                                 LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
                                 ARCHIVE DESTINATION "${LIB_INSTALL_DIR}")

set(
  PLATFORM_API_IMPLEMENTATION
  hybris
  CACHE
  STRING
  "API implementation to build (options are 'hybris', 'mirclient', or 'mirserver')"
)

if (PLATFORM_API_IMPLEMENTATION STREQUAL "mirclient")
  find_package( PkgConfig ) 
  pkg_check_modules( MIR_CLIENT mirclient )
endif()

if (PLATFORM_API_IMPLEMENTATION STREQUAL "mirserver")
  find_package( PkgConfig ) 
  pkg_check_modules( MIR_SERVER mirserver )
endif()

include_directories(include/)

add_subdirectory(src/)
add_subdirectory(include/)
