find_package(PkgConfig)
pkg_check_modules(PC_MirClient QUIET mirclient)

find_path(MirClient_INCLUDE_DIR mir_toolkit/mir_client_library.h
          HINTS ${PC_MirClient_INCLUDEDIR} ${PC_MirClient_INCLUDE_DIRS})

find_path(MirCommon_INCLUDE_DIR mir_toolkit/mir_native_buffer.h
          HINTS ${PC_MirClient_INCLUDEDIR} ${PC_MirClient_INCLUDE_DIRS})
                                
find_library(MirClient_LIBRARY libmirclient.so.0)

set(MirClient_LIBRARIES ${MirClient_LIBRARY})
set(MirClient_INCLUDE_DIRS ${MirClient_INCLUDE_DIR} ${MirCommon_INCLUDE_DIR})
message("zub ${MirClient_INCLUDE_DIRS}")

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set MirClient_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(MirClient  DEFAULT_MSG
                                  MirClient_LIBRARY MirClient_INCLUDE_DIRS)

mark_as_advanced(MirCommon_INCLUDE_DIR MirClient_INCLUDE_DIR MirClient_LIBRARY)
mark_as_advanced(MirClient_INCLUDE_DIR MirClient_LIBRARY)
