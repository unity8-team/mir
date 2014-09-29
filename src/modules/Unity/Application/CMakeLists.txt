include_directories(
    ${GLIB_INCLUDE_DIRS}
    ${MIRCOMMON_INCLUDE_DIRS}
    ${MIRSERVER_INCLUDE_DIRS}
    ${PROCESS_CPP_INCLUDE_DIRS}
    ${UBUNTU_PLATFORM_API_INCLUDE_DIRS}
    ${UBUNTU_APP_LAUNCH_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src/platforms/mirserver
    ${CMAKE_SOURCE_DIR}/src/common

    ${LTTNG_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${Qt5Qml_PRIVATE_INCLUDE_DIRS}
    ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
)

# We have to remove -pedantic in order to compile tracepoints.c
string (REPLACE " -pedantic " " " CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
# Needed to compile tracepoints in C99 mode.
add_definitions(-DBYTE_ORDER=__BYTE_ORDER)

set(QMLMIRPLUGIN_SRC
    application_manager.cpp
    application.cpp
    ../../../common/debughelpers.cpp
    desktopfilereader.cpp
    plugin.cpp
    applicationscreenshotprovider.cpp
    dbuswindowstack.cpp
    taskcontroller.cpp
    mirsurfacemanager.cpp
    ubuntukeyboardinfo.cpp
    mirsurfaceitem.cpp
    mirbuffersgtexture.cpp
    processcontroller.cpp
    proc_info.cpp
    session.cpp
    sessionmanager.cpp
    upstart/applicationcontroller.cpp
    tracepoints.c
# We need to run moc on these headers
    /usr/include/unity/shell/application/ApplicationInfoInterface.h
    /usr/include/unity/shell/application/ApplicationManagerInterface.h
# Feed the automoc monster
    session_interface.h
    applicationcontroller.h
    )

add_library(unityapplicationplugin SHARED
    ${QMLMIRPLUGIN_SRC}
)

target_link_libraries(
    unityapplicationplugin

    Qt5::Core
    Qt5::Quick
    Qt5::DBus

    ${CMAKE_THREAD_LIBS_INIT}

    ${GLIB_LDFLAGS}
    ${UBUNTU_PLATFORM_API_LDFLAGS}
    ${MIRSERVER_LDFLAGS}
    ${PROCESS_CPP_LDFLAGS}
    ${UBUNTU_APP_LAUNCH_LDFLAGS}
    ${LTTNG_LDFLAGS}

    ${GL_LIBRARIES}

    qpa-mirserver
)

qt5_use_modules(unityapplicationplugin DBus Qml)

# Generate tracepoints.c and .h from tracepoints.tp
include(UseLttngGenTp)
add_lttng_gen_tp(NAME tracepoints)

install(FILES "qmldir" DESTINATION "${CMAKE_INSTALL_LIBDIR}/${SHELL_INSTALL_QML}/Unity/Application")
install(TARGETS unityapplicationplugin LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/${SHELL_INSTALL_QML}/Unity/Application")
