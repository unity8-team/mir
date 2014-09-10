include(../../lttng-compiler.pri)

TARGET = qpa-mirserver
TEMPLATE = lib

QT -= gui
QT += core-private gui-private platformsupport-private dbus sensors

CONFIG += plugin
CONFIG += no_keywords  # "signals" clashes with Mir
CONFIG += qpa/genericunixfontdatabase

DEFINES += MESA_EGL_NO_X11_HEADERS

# CONFIG += c++11 # only enables C++0x
QMAKE_CXXFLAGS = -std=c++11 -Werror -Wall
QMAKE_LFLAGS = -std=c++11 -Wl,-no-undefined

INCLUDEPATH += ../../common

CONFIG   += link_pkgconfig
PKGCONFIG += mirserver protobuf egl xkbcommon url-dispatcher-1

LIBS += -lboost_system

SOURCES += \
    connectioncreator.cpp \
    ../../common/debughelpers.cpp \
    focussetter.cpp \
    qteventfeeder.cpp \
    plugin.cpp \
    qmirserver.cpp \
    sessionauthorizer.cpp \
    sessionlistener.cpp \
    surfaceconfigurator.cpp \
    promptsessionlistener.cpp \
    messageprocessor.cpp \
    mirplacementstrategy.cpp \
    mirserverconfiguration.cpp \
    mirserverstatuslistener.cpp \
    display.cpp \
    screen.cpp \
    displaywindow.cpp \
    mirserverintegration.cpp \
    miropenglcontext.cpp \
    nativeinterface.cpp \
    qtcompositor.cpp \
    services.cpp \
    ubuntutheme.cpp \
    unityprotobufservice.cpp \
    unityrpc.cpp

HEADERS += \
    connectioncreator.h \
    ../../common/debughelpers.h \
    focussetter.h \
    qteventfeeder.h \
    plugin.h \
    qmirserver.h \
    sessionauthorizer.h \
    sessionlistener.h \
    promptsessionlistener.h \
    surfaceconfigurator.h \
    logging.h \
    messageprocessor.h \
    mirglconfig.h \
    mirplacementstrategy.h \
    mirserverconfiguration.h \
    mirserverstatuslistener.h \
    display.h \
    screen.h \
    displaywindow.h \
    mirserverintegration.h \
    miropenglcontext.h \
    nativeinterface.h \
    qtcompositor.h \
    services.h \
    ubuntutheme.h \
    unityprotobufservice.h \
    unityrpc.h

LTTNG_TP_FILES += tracepoints.tp

# Installation path
target.path +=  $$[QT_INSTALL_PLUGINS]/platforms

INSTALLS += target
