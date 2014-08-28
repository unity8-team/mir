include(../../test-includes.pri)
include(../common/common.pri)

TARGET = general_tests

INCLUDEPATH += \
    ../../../src/platforms/mirserver

SOURCES += \
    mir_object_model_test.cpp \

# need to link in the QPA plugin too for access to MirServerConfiguration
LIBS += -Wl,-rpath,$${PWD}/../../../src/platforms/mirserver \
    -L../../../src/platforms/mirserver -lqpa-mirserver

