#
# This project tests the correctness of mirserver.pc.in, ensuring it contains
# everything an external project needs to successfully compile and link
# a mir server.
#
project(StandaloneServer)

# Forget everything you know about the parent "Mir" project. Pretend you're
# a distant third-party project. This ensures the directories we do get are
# coming from mirserver.pc ...
set_directory_properties(PROPERTIES
    INCLUDE_DIRECTORIES ""
    LINK_DIRECTORIES ""
)

# Generate a temporary mirserver.pc file which points into the build tree ...
set(LIBDIR ${Mir_BINARY_DIR}/lib)
set(INCLUDEDIR ${Mir_SOURCE_DIR}/include/server)
set(PLATFORM_INCLUDEDIR ${Mir_SOURCE_DIR}/include/platform)
set(COMMON_INCLUDEDIR ${Mir_SOURCE_DIR}/include/shared)
configure_file(${Mir_SOURCE_DIR}/src/server/mirserver.pc.in
               ${StandaloneServer_BINARY_DIR}/mirserver.pc)
set(ENV{PKG_CONFIG_PATH} ${StandaloneServer_BINARY_DIR})

# Now use mirserver.pc like real projects do, verifying it provides all
# the necessary information to build...
find_package(PkgConfig)
pkg_check_modules(LOCALMIRSERVER REQUIRED mirserver)

include_directories(${LOCALMIRSERVER_INCLUDE_DIRS})
link_directories(${LOCALMIRSERVER_LIBRARY_DIRS})
add_compile_options(${LOCALMIRSERVER_CFLAGS})

# Test CMake-style explicit linkage: "foo/libbar.so"
add_executable(test_standaloneserver_directlink server.cpp)
add_dependencies(test_standaloneserver_directlink mirserver)
target_link_libraries(test_standaloneserver_directlink ${LOCALMIRSERVER_LIBRARIES})

# Test traditional linkage: "-Lfoo -lbar"
add_executable(test_standaloneserver_searchlink server.cpp)
add_dependencies(test_standaloneserver_searchlink mirserver)
target_link_libraries(test_standaloneserver_searchlink ${LOCALMIRSERVER_LDFLAGS})

