# Variables defined by this module:
#   LIBHARDWARE_FOUND
#   LIBHARDWARE_INCLUDE_DIRS
#   LIBHARDWARE_LIBRARIES

INCLUDE(FindPackageHandleStandardArgs)

find_path(LIBHARDWARE_INCLUDE_DIR
   NAMES         hardware/hardware.h
                 hardware/gralloc.h
                 cutils/native_handle.h
                 system/graphics.h
                 system/window.h
   HINTS         /opt/bundle/junk
   )

find_library(LIBHARDWARE_LIBRARY
   NAMES         libhardware.so.1 
   HINTS         /opt/bundle/usr/lib
                /opt/bundle/usr/lib/arm-linux-gnueabihf
   )

set(LIBHARDWARE_LIBRARIES ${LIBHARDWARE_LIBRARY})
set(LIBHARDWARE_INCLUDE_DIRS ${LIBHARDWARE_INCLUDE_DIR})

# handle the QUIETLY and REQUIRED arguments and set LIBHARDWARE_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LIBHARDWARE DEFAULT_MSG
#                                  LIBHARDWARE_INCLUDE_DIR)
                                  LIBHARDWARE_LIBRARY LIBHARDWARE_INCLUDE_DIR)

mark_as_advanced(LIBHARDWARE_INCLUDE_DIR LIBHARDWARE_LIBRARY )
