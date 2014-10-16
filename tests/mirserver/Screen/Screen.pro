include(../../test-includes.pri)

TARGET = ScreenTest

QT += gui-private

INCLUDEPATH += \
    ../../../src/platforms/mirserver \
    ../../../src/common

SOURCES += \
    screen_test.cpp \
    ../../../src/common/debughelpers.cpp

LIBS += -Wl,-rpath,$${OUT_PWD}/../../../src/platforms/mirserver \
    -L../../../src/platforms/mirserver -lqpa-mirserver
