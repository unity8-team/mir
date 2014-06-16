TARGET = qpa-mirserver
TEMPLATE = lib

QT -= gui
QT += core-private gui-private platformsupport-private dbus sensors

CONFIG += plugin
CONFIG += no_keywords  # "signals" clashes with Mir
CONFIG += qpa/genericunixfontdatabase

# CONFIG += c++11 # only enables C++0x
QMAKE_CXXFLAGS = -std=c++11 -Werror -Wall
QMAKE_LFLAGS = -std=c++11 -Wl,-no-undefined

CONFIG   += link_pkgconfig
PKGCONFIG += mircommon mirserver mirclient egl xkbcommon url-dispatcher-1

LIBS += -lboost_system

SOURCES += \
    connectioncreator.cpp \
    qteventfeeder.cpp \
    plugin.cpp \
    qmirserver.cpp \
    sessionauthorizer.cpp \
    sessionlistener.cpp \
    surfaceconfigurator.cpp \
    messageprocessor.cpp \
    mirinputdispatcherconfiguration.cpp \
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
    ../common/ubuntutheme.cpp \
    unityprotobufservice.cpp \
    unityrpc.cpp

HEADERS += \
    connectioncreator.h \
    qteventfeeder.h \
    plugin.h \
    qmirserver.h \
    sessionauthorizer.h \
    sessionlistener.h \
    surfaceconfigurator.h \
    logging.h \
    messageprocessor.h \
    mirinputdispatcherconfiguration.h \
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
    ../common/ubuntutheme.h \
    unityprotobufservice.h \
    unityrpc.h

# Installation path
target.path +=  $$[QT_INSTALL_PLUGINS]/platforms

INSTALLS += target
