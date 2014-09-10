# Add extra compiler to handle LTTng TP files
# Append the files to the LTTNG_TP_FILES list

# Note by Gerry (Aug 2014) - I tried to have lttng-gen-tp generate an object file but it failed to link with
# the shared library we want. If there's a way to pass -fPIC to lttng-gen-tp, this workaround can be avoided
QMAKE_EXTRA_COMPILERS += lttng_gen_tp_header
lttng_gen_tp_header.name = "Generating headers .TP files and adding to HEADERS list"
lttng_gen_tp_header.input = LTTNG_TP_FILES
lttng_gen_tp_header.output = $${OUT_PWD}/${QMAKE_FILE_BASE}.h
lttng_gen_tp_header.commands = lttng-gen-tp -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
lttng_gen_tp_header.dependency_type = TYPE_H
lttng_gen_tp_header.variable_out = HEADERS #appends the generated file name to the HEADERS list
lttng_gen_tp_header.CONFIG = no_link target_predeps

QMAKE_EXTRA_COMPILERS += lttng_gen_tp_sources
lttng_gen_tp_sources.name = "Generating sources from .TP files and adding to SOURCES list"
lttng_gen_tp_sources.input = LTTNG_TP_FILES
lttng_gen_tp_sources.output = $${OUT_PWD}/${QMAKE_FILE_BASE}.c
lttng_gen_tp_sources.commands = lttng-gen-tp -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
lttng_gen_tp_sources.dependency_type = TYPE_C
lttng_gen_tp_sources.variable_out = SOURCES #appends the generated file name to the SOURCES list
lttng_gen_tp_sources.CONFIG = no_link target_predeps

CONFIG    += link_pkgconfig
PKGCONFIG += lttng-ust
