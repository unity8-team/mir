PKGCONFIG += mircommon mirserver ubuntu-platform-api

LIBS += -lubuntu_application_api_mirserver #FIXME platform-api pkgconfig should set this

SOURCES += \
    mirserver/initialsurfaceplacementstrategy.cpp \
    mirserver/qmirserver.cpp \
    mirserver/sessionauthorizer.cpp \
    mirserver/sessionlistener.cpp \
    mirserver/shellserverconfiguration.cpp \
    mirserver/surfaceconfigurator.cpp

HEADERS += \
    mirserver/initialsurfaceplacementstrategy.h \
    mirserver/qmirserver.h \
    mirserver/sessionauthorizer.h \
    mirserver/sessionlistener.h \
    mirserver/shellserverconfiguration.h \
    mirserver/surfaceconfigurator.h \
    mirserver/logging.h

install_headers.files = mirserver/qmirserverapplication.h \
    mirserver/qmirserver.h \
    mirserver/initialsurfaceplacementstrategy.h \
    mirserver/sessionauthorizer.h \
    mirserver/sessionlistener.h \
    mirserver/shellserverconfiguration.h \
    mirserver/surfacebuilder.h \
    mirserver/surfacefactory.h
install_headers.path = /usr/include/unity-mir

INSTALLS += install_headers
