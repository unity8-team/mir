include(../../test-includes.pri)
include(../common/common.pri)

TARGET = surface_manager_test

INCLUDEPATH += \
    ../../../src/platforms/mirserver

SOURCES += \
    surface_manager_test.cpp \
    surface_test.cpp

# need to link in the QPA plugin too for access to MirServerConfiguration
LIBS += -Wl,-rpath,$${PWD}/../../../src/platforms/mirserver \
    -L../../../src/platforms/mirserver -lqpa-mirserver

