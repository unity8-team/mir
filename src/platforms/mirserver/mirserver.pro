TARGET = qpa-mirserver
TEMPLATE = lib

QT -= gui
QT += core-private gui-private platformsupport-private dbus sensors

CONFIG += plugin
CONFIG += no_keywords  # "signals" clashes with Mir
CONFIG += qpa/genericunixfontdatabase

# CONFIG += c++11 # only enables C++0x
QMAKE_CXXFLAGS = -std=c++11
QMAKE_CXXFLAGS_RELEASE += -Werror  # so no stop on warning in debug builds
QMAKE_LFLAGS = -std=c++11 -Wl,-no-undefined

CONFIG   += link_pkgconfig
PKGCONFIG += mircommon mirserver mirclient egl

SOURCES += \
    qteventfeeder.cpp \
    plugin.cpp \
    qmirserver.cpp \
    sessionauthorizer.cpp \
    sessionlistener.cpp \
    surfaceconfigurator.cpp \
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
    dbusscreen.cpp \
    qtcompositor.cpp \
    ../common/ubuntutheme.cpp

HEADERS += \
    qteventfeeder.h \
    plugin.h \
    qmirserver.h \
    sessionauthorizer.h \
    sessionlistener.h \
    surfaceconfigurator.h \
    logging.h \
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
    dbusscreen.h \
    qtcompositor.h \
    ../common/ubuntutheme.h


# Installation path
target.path +=  $$[QT_INSTALL_PLUGINS]/platforms

INSTALLS += target
