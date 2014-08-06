
# FIXME: Can't run pkg-config build testing when cross-compiling. It tries
#        to use a bunch of paths under MIR_NDK_PATH that actually don't exist.
#        That probably needs fixing in our cross-compile setup scripts, but
#        so long as these test projects get attempted on native builds,
#        that is sufficient test coverage.

if (NOT CMAKE_FIND_ROOT_PATH_MODE_PROGRAM MATCHES "NEVER")
  add_subdirectory(standaloneserver)
  add_subdirectory(standaloneclient)
endif()
