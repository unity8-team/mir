GMOCK_SOURCES = /usr/src/gmock/src/gmock-all.cc \
                /usr/src/gmock/src/gmock_main.cc \
                /usr/src/gtest/src/gtest-all.cc

QMAKE_EXTRA_COMPILERS += gmock_compiler
gmock_compiler.input = GMOCK_SOURCES
gmock_compiler.output = $${OUT_PWD}/${QMAKE_FILE_BASE}.o
gmock_compiler.commands = g++ \
                          -I/usr/src/gtest \
                          -I/usr/src/gmock \
                          -c ${QMAKE_FILE_IN} \
                          -o ${QMAKE_FILE_OUT}
gmock_compiler.CONFIG = no_link

QMAKE_EXTRA_COMPILERS += gmock_linker
gmock_linker.depends = $${OUT_PWD}/gmock-all.o \
                       $${OUT_PWD}/gmock_main.o \
                       $${OUT_PWD}/gtest-all.o
gmock_linker.input = GMOCK_SOURCES
gmock_linker.output = $${OUT_PWD}/libgmock.a
gmock_linker.commands = ar -rv ${QMAKE_FILE_OUT} $${OUT_PWD}/gmock-all.o $${OUT_PWD}/gmock_main.o $${OUT_PWD}/gtest-all.o
gmock_linker.CONFIG = combine explicit_dependencies no_link target_predeps

LIBS += $${OUT_PWD}/libgmock.a
