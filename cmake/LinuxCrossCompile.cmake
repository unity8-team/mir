set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)

set(MIR_NDK_PATH $ENV{MIR_NDK_PATH} CACHE STRING "path of mir android bundle")
set(MIR_ARM_EABI "arm-linux-gnueabihf")
set (MIR_USES_BIONIC false CACHE BOOL "use the bionic libc/libstdc++")

set(CMAKE_C_COMPILER   /usr/bin/${MIR_ARM_EABI}-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/${MIR_ARM_EABI}-g++)

# where to look to find dependencies in the target environment
set(CMAKE_FIND_ROOT_PATH  "${MIR_NDK_PATH}")

#treat the chroot's includes as system includes
include_directories(SYSTEM ${MIR_NDK_PATH}/usr/include)
list(APPEND CMAKE_SYSTEM_INCLUDE_PATH "${MIR_NDK_PATH}/usr/include")

# Add the chroot libraries as system libraries
list(APPEND CMAKE_SYSTEM_LIBRARY_PATH
  "${MIR_NDK_PATH}/lib"
  "${MIR_NDK_PATH}/lib/${MIR_ARM_EABI}"
  "${MIR_NDK_PATH}/usr/lib"
  "${MIR_NDK_PATH}/usr/lib/${MIR_ARM_EABI}"
)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_EXECUTABLE_RUNTIME_C_FLAG "-Wl,-rpath-link,")
set(CMAKE_EXECUTABLE_RUNTIME_CXX_FLAG "-Wl,-rpath-link,")
set(CMAKE_INSTALL_RPATH "${MIR_NDK_PATH}/lib:${MIR_NDK_PATH}/lib/${MIR_ARM_EABI}:${MIR_NDK_PATH}/usr/lib:${MIR_NDK_PATH}/usr/lib/${MIR_ARM_EABI}")

#use only the cross compile system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
