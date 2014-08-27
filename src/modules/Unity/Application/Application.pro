TARGET = unityapplicationplugin
TEMPLATE = lib

QT       += core quick dbus
QT       += quick-private qml-private core-private
QT       += gui-private # annoyingly needed by included NativeInterface
CONFIG   += link_pkgconfig plugin debug no_keywords # keywords clash with ProcessC++

# CONFIG += c++11 # only enables C++0x
QMAKE_CXXFLAGS = -std=c++11 -Werror -Wall
QMAKE_LFLAGS = -std=c++11 -Wl,-no-undefined

PKGCONFIG += mirserver glib-2.0 process-cpp ubuntu-app-launch-2

INCLUDEPATH += ../../../platforms/mirserver ../../../common
LIBS += -L../../../platforms/mirserver -lqpa-mirserver
QMAKE_RPATHDIR += $$[QT_INSTALL_PLUGINS]/platforms # where libqpa-mirserver.so is installed

# workaround subdir depends not being aggressive enough
PRE_TARGETDEPS += $${OUT_PWD}/../../../platforms/mirserver/libqpa-mirserver.so

TARGET = $$qtLibraryTarget($$TARGET)
uri = Unity.Application

SOURCES += application_manager.cpp \
    application.cpp \
    ../../../common/debughelpers.cpp \
    desktopfilereader.cpp \
    plugin.cpp \
    applicationscreenshotprovider.cpp \
    dbuswindowstack.cpp \
    taskcontroller.cpp \
    mirsurfacemanager.cpp \
    ubuntukeyboardinfo.cpp \
    mirsurfaceitem.cpp \
    mirbuffersgtexture.cpp \
    processcontroller.cpp \
    proc_info.cpp \
    upstart/applicationcontroller.cpp \


HEADERS += application_manager.h \
    applicationcontroller.h \
    application.h \
    ../../../common/debughelpers.h \
    desktopfilereader.h \
    applicationscreenshotprovider.h \
    dbuswindowstack.h \
    taskcontroller.h \
    mirsurfacemanager.h \
    ubuntukeyboardinfo.h \
    /usr/include/unity/shell/application/ApplicationManagerInterface.h \
    /usr/include/unity/shell/application/ApplicationInfoInterface.h \
    mirsurfaceitem.h \
    mirbuffersgtexture.h \
    processcontroller.h \
    proc_info.h \
    upstart/applicationcontroller.h

installPath = $$[QT_INSTALL_QML]/$$replace(uri, \\., /)

QML_FILES = qmldir
qml_files.path = $$installPath
qml_files.files = $$QML_FILES

target.path = $$installPath

INSTALLS += target qml_files
