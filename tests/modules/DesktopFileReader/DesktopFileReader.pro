include(../../test-includes.pri)
include(../common/common.pri)

TARGET = desktopfilereader_test

SOURCES += \
    desktopfilereader_test.cpp

OTHER_FILES += \
    calculator.desktop

# Copy to build directory
for(FILE, OTHER_FILES){
    QMAKE_POST_LINK += $$quote(cp $${PWD}/$${FILE} $${OUT_PWD}$$escape_expand(\\n\\t))
}
