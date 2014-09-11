include(../../test-includes.pri)
include(../common/common.pri)

TARGET = mirsurfaceitem_test

QT += testlib

INCLUDEPATH += \
    ../../../src/platforms/mirserver \
    ../../../src/modules/Unity/Application

SOURCES += \
    mirsurfaceitem_test.cpp
