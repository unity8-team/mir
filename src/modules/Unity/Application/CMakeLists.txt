# TODO Cflags?
# export_qmlplugin macro
include(Plugins)

find_package(Qt5Core)
find_package(Qt5Gui)
find_package(Qt5Qml)
find_package(Qt5Quick)

pkg_check_modules(LTTNG lttng-ust)

#pkg_check_modules(APPLICATION_API REQUIRED unity-shell-application=2)

pkg_check_modules(GLIB glib-2.0 REQUIRED)
pkg_check_modules(PROCESS_CPP process-cpp REQUIRED)
pkg_check_modules(UBUNTU_APP_LAUNCH ubuntu-app-launch-2 REQUIRED)

add_definitions(-DQT_PLUGIN)

add_definitions(-DQT_NO_KEYWORDS)

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

# We have to remove -pedantic for tracepoints.c
string (REPLACE " -pedantic " " " CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
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
        # Dirty hacks.
    /usr/include/unity/shell/application/ApplicationInfoInterface.h
    /usr/include/unity/shell/application/ApplicationManagerInterface.h
    tracepoints.c
#    session_interface.moc
# Hacks to feed the automoc monster
session_interface.h
applicationcontroller.h
    )

# TODO: Module?
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
#    ${CMAKE_BINARY_DIR}/src/platforms/mirserver/libmirserver.so
    qpa-mirserver
)

#target_link_libraries(unityapplicationplugin ${GSETTINGS_QT_LDFLAGS})

#qt5_use_modules(unityapplicationplugin DBus Qml)

# export the qmldir and qmltypes files
#export_qmlfiles(Unity Unity)
#export_qmlplugin(Unity 0.1 Unity TARGETS unityapplicationplugin)

# Generate tracepoints.c and .h from tracepoints.tp
include(UseLttngGenTp)
add_lttng_gen_tp(NAME tracepoints)

set(QML_MODULE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/qt5/qml")
install(FILES "qmldir" DESTINATION "${QML_MODULE_INSTALL_DIR}/Unity/Application")
install(TARGETS unityapplicationplugin LIBRARY DESTINATION "${QML_MODULE_INSTALL_DIR}/Unity/Application")