include(../../test-includes.pri)

TARGET = QtEventFeederTest

QT += gui-private

INCLUDEPATH += \
    ../../../src/platforms/mirserver \
    ../../../src/common

SOURCES += \
    qteventfeeder_test.cpp \
    ../../../src/common/debughelpers.cpp

LIBS += -Wl,-rpath,$${OUT_PWD}/../../../src/platforms/mirserver \
    -L../../../src/platforms/mirserver -lqpa-mirserver
