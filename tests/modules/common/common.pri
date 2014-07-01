CONFIG += link_pkgconfig no_keywords # keywords clash with ProcessC++
PKGCONFIG += mirserver process-cpp ubuntu-app-launch-2

QT += quick testlib
QMAKE_CXXFLAGS = -std=c++11

HEADERS += ../common/mock_application_controller.h \
    ../common/mock_desktop_file_reader.h \
    ../common/mock_focus_controller.h \
    ../common/mock_oom_controller.h \
    ../common/mock_process_controller.h \
    ../common/mock_proc_info.h \
    ../common/mock_session.h \
    ../../../src/modules/Unity/Application/applicationcontroller.h \
    ../../../src/modules/Unity/Application/processcontroller.h \
    ../../../src/modules/Unity/Application/taskcontroller.h

INCLUDEPATH += ../../../src/modules \
    ../common


GMOCK_SOURCES = /usr/src/gmock/src/gmock-all.cc \
                /usr/src/gmock/src/gmock_main.cc \
                /usr/src/gtest/src/gtest-all.cc

QMAKE_EXTRA_COMPILERS += gmock_compiler
gmock_compiler.input = GMOCK_SOURCES
gmock_compiler.output = $${PWD}/${QMAKE_FILE_BASE}.o
gmock_compiler.commands = g++ \
                          -I/usr/src/gtest \
                          -I/usr/src/gmock \
                          -c ${QMAKE_FILE_IN} \
                          -o ${QMAKE_FILE_OUT}
gmock_compiler.CONFIG = no_link

QMAKE_EXTRA_COMPILERS += gmock_linker
gmock_linker.depends = $${PWD}/gmock-all.o \
                       $${PWD}/gmock_main.o \
                       $${PWD}/gtest-all.o
gmock_linker.input = GMOCK_SOURCES
gmock_linker.output = $${PWD}/libgmock.a
gmock_linker.commands = ar -rv ${QMAKE_FILE_OUT} $${PWD}/gmock-all.o $${PWD}/gmock_main.o $${PWD}/gtest-all.o
gmock_linker.CONFIG = combine explicit_dependencies no_link target_predeps

LIBS += $${PWD}/libgmock.a \
    -Wl,-rpath,$${PWD}/../../../src/modules/Unity/Application \
    -L../../../src/modules/Unity/Application -lunityapplicationplugin
