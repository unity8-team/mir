TARGET = qpa-mirserver
TEMPLATE = lib

QT -= gui
QT += core-private gui-private platformsupport-private

CONFIG += plugin
CONFIG += no_keywords  # "signals" clashes with Mir

# CONFIG += c++11 # only enables C++0x
QMAKE_CXXFLAGS = -std=c++11
QMAKE_CXXFLAGS_RELEASE += -Werror  # so no stop on warning in debug builds
QMAKE_LFLAGS = -std=c++11 -Wl,-no-undefined

CONFIG   += link_pkgconfig
PKGCONFIG += mircommon mirserver egl

SOURCES += plugin.cpp \
    qmirserver.cpp \
    sessionauthorizer.cpp \
    sessionlistener.cpp \
    surfaceconfigurator.cpp \
    mirserverconfiguration.cpp \
    display.cpp \
    screen.cpp \
    displaywindow.cpp \
    mirserverintegration.cpp \
    miropenglcontext.cpp \
    voidcompositor.cpp \
    nativeinterface.cpp

HEADERS += plugin.h \
    qmirserver.h \
    sessionauthorizer.h \
    sessionlistener.h \
    surfaceconfigurator.h \
    logging.h \
    mirserverconfiguration.h \
    display.h \
    screen.h \
    displaywindow.h \
    mirserverintegration.h \
    miropenglcontext.h \
    voidcompositor.h \
    nativeinterface.h

# Installation path
target.path +=  $$[QT_INSTALL_PLUGINS]/platforms

INSTALLS += target
