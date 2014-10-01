include(../../test-includes.pri)

TARGET = ClipboardTest

QT += gui-private

INCLUDEPATH += \
    ../../../src/platforms/mirserver \
    ../../../src/common

SOURCES += \
    clipboard_test.cpp \
    ../../../src/common/debughelpers.cpp

LIBS += -Wl,-rpath,$${OUT_PWD}/../../../src/platforms/mirserver \
    -L../../../src/platforms/mirserver -lqpa-mirserver
