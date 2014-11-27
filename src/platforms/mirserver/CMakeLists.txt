# Qt5PlatformSupport is not available as a cmake module.
pkg_check_modules(QT5PLATFORM_SUPPORT Qt5PlatformSupport REQUIRED)

# Hacks for the QPA privates monster.
pkg_check_modules(FONTCONFIG fontconfig REQUIRED)
add_definitions(-DQ_FONTCONFIGDATABASE)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -fPIC")

# Dirty hack: The private QtPlatformSupport include dirs are not made available via
# a cmake module or the pkgconfig file (which only exposes the necessary linker flags)
# So we generate the paths ourselves from the Qt5Gui private dirs!
set(QT5_PLATFORMSUPPORT_INCLUDE_DIRS)
foreach(item ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
  set(newitem "")
   string(REGEX REPLACE "QtGui" "QtPlatformSupport" newitem ${item})
   list(APPEND QT5_PLATFORMSUPPORT_INCLUDE_DIRS ${newitem})
endforeach(item ${Qt5Gui_PRIVATE_INCLUDE_DIRS})

include_directories(
    ${MIRCOMMON_INCLUDE_DIRS}
    ${MIRSERVER_INCLUDE_DIRS}
    
    ${URL_DISPATCHER_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${LTTNG_INCLUDE_DIRS}

    ${QT5PLATFORM_SUPPORT_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${QT5_PLATFORMSUPPORT_INCLUDE_DIRS}
)

# We have to remove -pedantic for tracepoints.c
string (REPLACE " -pedantic " " " CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
# Needed to compile tracepoints in C99 mode.
add_definitions(-DBYTE_ORDER=__BYTE_ORDER)

set(MIRSERVER_QPA_PLUGIN_SRC
    ../../common/debughelpers.cpp
    focussetter.cpp
    qteventfeeder.cpp
    plugin.cpp
    qmirserver.cpp
    sessionauthorizer.cpp
    sessionlistener.cpp
    surfaceconfigurator.cpp
    promptsessionlistener.cpp
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
    clipboard.cpp
    tracepoints.c
    )

add_library(qpa-mirserver SHARED
    ${MIRSERVER_QPA_PLUGIN_SRC}
)

target_link_libraries(
    qpa-mirserver

    ${MIRSERVER_LDFLAGS}
    ${URL_DISPATCHER_LDFLAGS}
    ${PROTOBUF_LDFLAGS}
    ${EGL_LDFLAGS}
    ${GL_LIBRARIES}
    ${LTTNG_LIBRARIES}
    
    ${QT5PLATFORM_SUPPORT_LDFLAGS}
    # TODO Qt5Platform support LDFLAGS dont provide actual required ldflags...
    # I found these were needed...perhaps there is some way to query qmake/qconfig?
    -lfreetype 
    ${GIO_LDFLAGS}
    ${FONTCONFIG_LDFLAGS}

    -lboost_system
)

qt5_use_modules(qpa-mirserver Core Quick DBus Sensors)

include(UseLttngGenTp)
add_lttng_gen_tp(NAME tracepoints)

install(TARGETS qpa-mirserver LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/qt5/plugins/platforms")
