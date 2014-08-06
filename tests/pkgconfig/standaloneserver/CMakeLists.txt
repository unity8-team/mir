#
# This project tests the correctness of mirserver.pc.in, ensuring it contains
# everything an external project needs to successfully compile and link
# a mir server.
#
project(StandaloneServer)

# Generate a temporary mirserver.pc file which points into the build tree ...
set(LIBDIR ${Mir_BINARY_DIR}/lib)
set(INCLUDEDIR ${Mir_SOURCE_DIR}/include/mirserver)
set(PLATFORM_INCLUDEDIR ${Mir_SOURCE_DIR}include/mirplatform)
set(COMMON_INCLUDEDIR ${Mir_SOURCE_DIR}include/mircommon)
configure_file(${Mir_SOURCE_DIR}/src/server/mirserver.pc.in
               ${StandaloneServer_BINARY_DIR}/mirserver.pc)
set(ENV{PKG_CONFIG_PATH} ${StandaloneServer_BINARY_DIR})

# ---8<--- An external Mir server project looks like this ---8<---
find_package(PkgConfig)
pkg_check_modules(LOCALMIRSERVER REQUIRED mirserver)

add_executable(test_standaloneserver ${Mir_SOURCE_DIR}/examples/minimal_server.cpp)
target_link_libraries(test_standaloneserver ${LOCALMIRSERVER_LIBRARIES})

