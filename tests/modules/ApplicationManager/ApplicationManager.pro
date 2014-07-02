TEMPLATE = app
TARGET = application_manager_test
CONFIG += testcase

include(../common/common.pri)

INCLUDEPATH += \
    ../../../src/platforms/mirserver

SOURCES += \
    application_manager_test.cpp

# need to link in the QPA plugin too for access to MirServerConfiguration
LIBS += -Wl,-rpath,$${PWD}/../../../src/platforms/mirserver \
    -L../../../src/platforms/mirserver -lqpa-mirserver

