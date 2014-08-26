include(../../test-includes.pri)
include(../common/common.pri)

TARGET = application_manager_test

INCLUDEPATH += \
    ../../../src/platforms/mirserver \
    ../../../src/modules/Unity/Application

SOURCES += \
    application_manager_test.cpp \
    application_test.cpp

# need to link in the QPA plugin too for access to MirServerConfiguration
LIBS += -Wl,-rpath,$${PWD}/../../../src/platforms/mirserver \
    -L../../../src/platforms/mirserver -lqpa-mirserver

