set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)

set(MIR_ARM_EABI "arm-linux-gnueabihf")

set(CMAKE_C_COMPILER   /usr/bin/${MIR_ARM_EABI}-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/${MIR_ARM_EABI}-g++)

#treat the chroot's includes as system includes
include_directories(SYSTEM "/usr/include" "/usr/include/${MIR_ARM_EABI}")
list(APPEND CMAKE_SYSTEM_INCLUDE_PATH "/usr/include" "/usr/include/${MIR_ARM_EABI}" )

# Add the chroot libraries as system libraries
list(APPEND CMAKE_SYSTEM_LIBRARY_PATH
  "/lib"
  "/lib/${MIR_ARM_EABI}"
  "/usr/lib"
  "/usr/lib/${MIR_ARM_EABI}"
)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_EXECUTABLE_RUNTIME_C_FLAG "-Wl,-rpath-link,")
set(CMAKE_EXECUTABLE_RUNTIME_CXX_FLAG "-Wl,-rpath-link,")
set(CMAKE_INSTALL_RPATH "/lib:/lib/${MIR_ARM_EABI}:/usr/lib:/usr/lib/${MIR_ARM_EABI}")

set(ENV{PKG_CONFIG_PATH} "/usr/lib/pkgconfig:/usr/lib/${MIR_ARM_EABI}/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
