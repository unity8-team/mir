#
# This project tests the correctness of mirclient.pc.in, ensuring it contains
# everything an external project needs to successfully compile and link
# a mir client.
#
project(StandaloneClient)

# Forget everything you know about the parent "Mir" project. Pretend you're
# a distant third-party project. This ensures the directories we do get are
# coming from mirclient.pc ...
set_directory_properties(PROPERTIES
    INCLUDE_DIRECTORIES ""
    LINK_DIRECTORIES ""
)

# Generate a temporary mirclient.pc file which points into the build tree ...
set(LIBDIR ${Mir_BINARY_DIR}/lib)
set(INCLUDEDIR ${Mir_SOURCE_DIR}/include/client)
set(COMMON_INCLUDEDIR ${Mir_SOURCE_DIR}/include/shared)
configure_file(${Mir_SOURCE_DIR}/src/client/mirclient.pc.in
               ${StandaloneClient_BINARY_DIR}/mirclient.pc)
set(ENV{PKG_CONFIG_PATH} ${StandaloneClient_BINARY_DIR})

# Now use mirclient.pc like real projects do, verifying it provides all
# the necessary information to build...
find_package(PkgConfig)
pkg_check_modules(LOCALMIRCLIENT REQUIRED mirclient)

include_directories(${LOCALMIRCLIENT_INCLUDE_DIRS})
link_directories(${LOCALMIRCLIENT_LIBRARY_DIRS})
add_compile_options(${LOCALMIRCLIENT_CFLAGS})

set(CLIENT_SRC "${Mir_SOURCE_DIR}/examples/flicker.c")
add_compile_options("-std=c99")

# Test CMake-style explicit linkage: "foo/libbar.so"
add_executable(test_standaloneclient_directlink ${CLIENT_SRC})
add_dependencies(test_standaloneclient_directlink mirclient)
target_link_libraries(test_standaloneclient_directlink ${LOCALMIRCLIENT_LIBRARIES})

# Test traditional linkage: "-Lfoo -lbar"
add_executable(test_standaloneclient_searchlink ${CLIENT_SRC})
add_dependencies(test_standaloneclient_searchlink mirclient)
target_link_libraries(test_standaloneclient_searchlink ${LOCALMIRCLIENT_LDFLAGS})

