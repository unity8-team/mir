TARGET = qpa-ubuntuclient
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

INCLUDEPATH += "../common"

CONFIG   += link_pkgconfig
PKGCONFIG += mircommon mirclient egl

LIBS += -lubuntu_application_api_mirclient

SOURCES += \
    plugin.cpp \
    ubuntubackingstore.cpp \
    ubuntuclientintegration.cpp \
    ubuntuclipboard.cpp \
    ubuntuinput.cpp \
    ubuntunativeinterface.cpp \
    ubuntuopenglcontext.cpp \
    ubuntuplatformservices.cpp \
    ubuntuscreen.cpp \
    ../common/ubuntutheme.cpp \
    ubuntuwindow.cpp \


HEADERS += \
    plugin.h \
    ubuntubackingstore.h \
    ubuntuclientintegration.h \
    ubuntuclipboard.h \
    ubuntuinput.h \
    ubuntunativeinterface.h \
    ubuntuopenglcontext.h \
    ubuntuplatformservices.h \
    ubuntuscreen.h \
    ../common/ubuntutheme.h \
    ubuntuwindow.h \


# Installation path
target.path +=  $$[QT_INSTALL_PLUGINS]/platforms

INSTALLS += target
