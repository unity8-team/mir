include(../../test-includes.pri)
include(../common/common.pri)

TARGET = application_test

INCLUDEPATH += \
    ../../../src/platforms/mirserver

SOURCES += \
    application_test.cpp \
    surface_test.cpp

# need to link in the QPA plugin too for access to MirServerConfiguration
LIBS += -Wl,-rpath,$${PWD}/../../../src/platforms/mirserver \
    -L../../../src/platforms/mirserver -lqpa-mirserver

