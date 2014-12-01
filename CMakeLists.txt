cmake_minimum_required(VERSION 2.8.9)

project(qtmir)

set(QTMIR_VERSION_MAJOR 0)
set(QTMIR_VERSION_MINOR 1)
set(QTMIR_VERSION_PATCH 0)

if(${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
   message(FATAL_ERROR "In-tree build attempt detected, aborting. Set your build dir outside your source dir, delete CMakeCache.txt and CMakeFiles/ from source root and try again.")
endif()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# add custom cmake modules
# cmake-extras module path - not equal to ${CMAKE_ROOT}/Modules due to bug:1394648
set(CMAKE_EXTRAS_MODULE_PATH /usr/share/cmake-2.8/Modules)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_EXTRAS_MODULE_PATH})

# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fno-strict-aliasing -Werror -Wextra")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")


include(EnableCoverageReport)
#####################################################################
# Enable code coverage calculation with gcov/gcovr/lcov
# Usage:
#  * Switch build type to coverage (use ccmake or cmake-gui)
#  * Invoke make, make test, make coverage (or ninja if you use that backend)
#  * Find html report in subdir coveragereport
#  * Find xml report feasible for jenkins in coverage.xml
#####################################################################
if(cmake_build_type_lower MATCHES coverage)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage" )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage" )
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} --coverage" )
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage" )
    ENABLE_COVERAGE_REPORT(TARGETS ${SHELL_APP} FILTER /usr/include ${CMAKE_SOURCE_DIR}/tests/* ${CMAKE_BINARY_DIR}/*)
endif()

# Make sure we have all the needed symbols
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-z,defs")

# Static C++ checks
add_custom_target(cppcheck COMMAND cppcheck --enable=all -q --error-exitcode=2
                                       ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)

include(FindPkgConfig)
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5DBus 5.3 REQUIRED)
find_package(Qt5Gui 5.3 REQUIRED)
find_package(Qt5Qml 5.3 REQUIRED)
find_package(Qt5Quick 5.3 REQUIRED)
find_package(Qt5Sensors 5.3 REQUIRED)
find_package(Qt5Test 5.3 REQUIRED)

find_package(Threads REQUIRED)
find_package(Boost 1.50 COMPONENTS system REQUIRED)

find_package(Protobuf REQUIRED)
if(PROTOBUF_PROTOC_EXECUTABLE STREQUAL "PROTOBUF_PROTOC_EXECUTABLE-NOTFOUND")
    message(SEND_ERROR "protoc executable not found! Missing protobuf-compiler package?")
endif()

pkg_check_modules(MIRSERVER mirserver REQUIRED)

pkg_check_modules(GLIB glib-2.0 REQUIRED)
pkg_check_modules(PROCESS_CPP process-cpp REQUIRED)
pkg_check_modules(UBUNTU_APP_LAUNCH ubuntu-app-launch-2 REQUIRED)
pkg_check_modules(URL_DISPATCHER url-dispatcher-1)
pkg_check_modules(PROTOBUF protobuf)
pkg_check_modules(EGL egl)
pkg_check_modules(GIO gio-2.0)
pkg_check_modules(GIO_UNIX gio-unix-2.0)
pkg_check_modules(LTTNG lttng-ust)


# We expect this to be set via debian/rules for GLES builds
if ("${USE_OPENGLES}" STREQUAL 1)
    message(STATUS "Qt5 determined to be compiled with GLES support")
    pkg_check_modules(GLESv2 glesv2)
    add_definitions(-DQT_USING_GLES)
    include_directories (${GLESv2_INCLUDE_DIRS})
    set (GL_LIBRARIES "${GLESv2_LIBRARIES}")
# Because qt/gl and qt/gles are not parallel installable we have a difficulty
# building qtmir-desktop on armhf. We would like to link against opengl and bind the
# opengl API but Qt is already linked against the ES APIs. So in this setup we link against OpenGLES
# to accomodate Qt but bind the OpenGL API anyway. This is technically broken though
# I suspect Mesa may allow it ~racarr.
elseif("${USE_OPENGL_BUT_LINK_AGAINST_OPENGLES}")
    message(STATUS "Linking against OpenGL ES but binding OpenGL API")
    pkg_check_modules(GLESv2 glesv2)
    add_definitions(-DQT_USING_GL)
    include_directories (${GLESv2_INCLUDE_DIRS})
    set (GL_LIBRARIES "${GLESv2_LIBRARIES}")
else()
    message(STATUS "Qt5 determined to be compiled with OpenGL support")
    pkg_check_modules(GL gl)
    add_definitions(-DQT_USING_OPENGL)
    include_directories (${GL_INCLUDE_DIRS})
endif()

# Standard install paths
include(GNUInstallDirs)


# Determine QPA plugin install path
# FIXME(greyback) there must be a better way than calling a private macro to set a property.
# I'm manually calling a macro from Qt5GuiConfig.cmake, but I don't understand why it isn't being called.
_populate_Gui_plugin_properties("Gui" "PLATFORMS" "platforms")
get_target_property(Qt5Gui_QPA_Plugin_Path Qt5::Gui IMPORTED_LOCATION_PLATFORMS)

# Set QML module install path
set(QML_MODULE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/qt5/qml")


# Customisations for the build type
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_lower)

if(cmake_build_type_lower MATCHES relwithdebinfo) # workaround for http://public.kitware.com/Bug/view.php?id=14696
    add_definitions(-DQT_NO_DEBUG)
endif()
if ("${CMAKE_BUILD_TYPE}" STREQUAL "release" OR "${CMAKE_BUILD_TYPE}" STREQUAL "relwithdebinfo")
    option(Werror "Treat warnings as errors" ON)
else()
    option(Werror "Treat warnings as errors" OFF)
endif()

if (Werror)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()


# Mir uses boost, which does not like Qt defining macros named "signals" and "slots"
add_definitions(-DQT_NO_KEYWORDS)

# Tests
include(CTest)
enable_testing()


# add subdirectories to build
add_subdirectory(src)
add_subdirectory(tests)
