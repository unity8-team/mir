TARGET = qpa-mirserver
TEMPLATE = lib

QT -= gui
QT += core-private gui-private platformsupport-private dbus

CONFIG += plugin
CONFIG += no_keywords  # "signals" clashes with Mir

# CONFIG += c++11 # only enables C++0x
QMAKE_CXXFLAGS = -std=c++11
QMAKE_CXXFLAGS_RELEASE += -Werror  # so no stop on warning in debug builds
QMAKE_LFLAGS = -std=c++11 -Wl,-no-undefined

CONFIG   += link_pkgconfig
PKGCONFIG += mircommon mirserver mirclient egl

INCLUDEPATH += "/usr/include/mirserver/android-deps"
INCLUDEPATH += "/usr/include/mirserver/android-input/android/frameworks/base/include"
INCLUDEPATH += "/usr/include/mirserver/android-input/android/frameworks/base/services/input"
INCLUDEPATH += "/usr/include/mirserver/android-input/android/frameworks/native/include"

QMAKE_CXXFLAGS += "-include /usr/include/mirserver/android-input/android/system/core/include/arch/ubuntu-x86/AndroidConfig.h"

SOURCES += \
    inputreaderpolicy.cpp \
    qteventfeeder.cpp \
    plugin.cpp \
    qmirserver.cpp \
    sessionauthorizer.cpp \
    sessionlistener.cpp \
    surfaceconfigurator.cpp \
    mirinputconfiguration.cpp \
    mirinputmanager.cpp \
    mirserverconfiguration.cpp \
    display.cpp \
    screen.cpp \
    displaywindow.cpp \
    mirserverintegration.cpp \
    miropenglcontext.cpp \
    voidcompositor.cpp \
    nativeinterface.cpp \
    dbusscreen.cpp

HEADERS += \
    inputreaderpolicy.h \
    qteventfeeder.h \
    plugin.h \
    qmirserver.h \
    sessionauthorizer.h \
    sessionlistener.h \
    surfaceconfigurator.h \
    logging.h \
    mirinputchannel.h \
    mirinputconfiguration.h \
    mirinputmanager.h \
    mirserverconfiguration.h \
    display.h \
    screen.h \
    displaywindow.h \
    mirserverintegration.h \
    miropenglcontext.h \
    voidcompositor.h \
    nativeinterface.h \
    dbusscreen.h

# Installation path
target.path +=  $$[QT_INSTALL_PLUGINS]/platforms

INSTALLS += target
