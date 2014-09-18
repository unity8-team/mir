#find_package(Qt5Gui)
#find_package(Qt5PlatformSupport)
#include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
#include_directories(${Qt5PlatformSupport_PRIVATE_INCLUDE_DIRS})

pkg_check_modules(URL_DISPATCHER url-dispatcher-1)
pkg_check_modules(PROTOBUF protobuf)
pkg_check_modules(EGL egl)
pkg_check_modules(GIO gio-2.0)
pkg_check_modules(LTTNG lttng-ust)

pkg_check_modules(QT5PLATFORM_SUPPORT Qt5PlatformSupport REQUIRED)

find_package(Qt5Gui)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -fPIC")

# TODO The private QtPlatformSupport include dirs are not made available via
# a cmake module or the pkgconfig file (which only exposes the necessary linker flags)
# So we generate the paths ourselves from the Qt5Gui private dirs!
set(QT5_SECRET_INCLUDE_DIRS)
foreach(item ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
  set(newitem "FOO")
   string(REGEX REPLACE "QtGui" "QtPlatformSupport" newitem ${item})
  message("item ${item} newitem ${newitem}")
   list(APPEND QT5_SECRET_INCLUDE_DIRS ${newitem})
endforeach(item ${Qt5Gui_PRIVATE_INCLUDE_DIRS})

add_definitions(-DQT_NO_KEYWORDS)

include_directories(
    ${MIRCOMMON_INCLUDE_DIRS}
    ${MIRSERVER_INCLUDE_DIRS}
    
    ${URL_DISPATCHER_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${LTTNG_INCLUDE_DIRS}

    ${QT5PLATFORM_SUPPORT_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${QT5_SECRET_INCLUDE_DIRS}
)

# We have to remove -pedantic for tracepoints.c
string (REPLACE " -pedantic " " " CMAKE_C_FLAGS ${CMAKE_C_FLAGS})

set(MIRSERVER_QPA_PLUGIN_SRC
    connectioncreator.cpp
    ../../common/debughelpers.cpp
    focussetter.cpp
    qteventfeeder.cpp
    plugin.cpp
    qmirserver.cpp
    sessionauthorizer.cpp
    sessionlistener.cpp
    surfaceconfigurator.cpp
    promptsessionlistener.cpp
    messageprocessor.cpp
    mirplacementstrategy.cpp
    mirserverconfiguration.cpp
    mirserverstatuslistener.cpp
    display.cpp
    screen.cpp
    displaywindow.cpp
    mirserverintegration.cpp
    miropenglcontext.cpp
    nativeinterface.cpp
    qtcompositor.cpp
    services.cpp
    ubuntutheme.cpp
    unityprotobufservice.cpp
    unityrpc.cpp
    tracepoints.c
    )

# TODO: Does this need to be module?
add_library(qpa-mirserver SHARED
    ${MIRSERVER_QPA_PLUGIN_SRC}
)

target_link_libraries(
    qpa-mirserver

    Qt5::Core
    Qt5::Quick
    Qt5::DBus

    ${MIRSERVER_LDFLAGS}
    ${URL_DISPATCHER_LDFLAGS}
    ${PROTOBUF_LDFLAGS}
    ${EGL_LDFLAGS}
    ${GL_LIBRARIES}
    ${LTTNG_LIBRARIES}
    
    ${QT5PLATFORM_SUPPORT_LDFLAGS}
    # TODO Qt5Platform support LDFLAGS dont provide much...
    -lfreetype # TODO ?
    ${GIO_LDFLAGS}

    -lboost_system
)

include(UseLttngGenTp)
add_lttng_gen_tp(NAME tracepoints)

set(QPA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/qt5/plugins/platforms")
install(FILES "${CMAKE_BINARY_DIR}/src/platforms/mirserver/libqpa-mirserver.so" DESTINATION "${QPA_INSTALL_DIR}")