TARGET = qpa-mirserver
TEMPLATE = lib

QT -= gui
QT += core-private gui-private platformsupport-private

CONFIG += plugin

# CONFIG += c++11 # only enables C++0x
QMAKE_CXXFLAGS = -std=c++11
QMAKE_CXXFLAGS_RELEASE += -fvisibility=hidden -fvisibility-inlines-hidden -Werror     # so no stop on warning in debug builds
QMAKE_LFLAGS = -std=c++11 -Wl,-no-undefined

SOURCES += plugin.cpp \
    mirserverintegration.cpp

HEADERS += plugin.h \
    mirserverintegration.h

# Installation path
target.path +=  $$[QT_INSTALL_PLUGINS]/platforms

INSTALLS += target
