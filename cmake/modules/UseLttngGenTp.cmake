cmake_minimum_required(VERSION 2.6)
if(POLICY CMP0011)
 cmake_policy(SET CMP0011 NEW)
endif(POLICY CMP0011)

find_program(LTTNG_GEN_TP NAMES lttng-gen-tp DOC "lttng-gen-tp executable")
if(NOT LTTNG_GEN_TP)
 message(FATAL_ERROR "Excutable lttng-gen-top not found")
endif()

function(add_lttng_gen_tp)
 set(_one_value NAME)
 cmake_parse_arguments (arg "" "${_one_value}" "" ${ARGN})

 add_custom_command(
 OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${arg_NAME}.h" "${CMAKE_CURRENT_BINARY_DIR}/${arg_NAME}.c"
 COMMAND "${LTTNG_GEN_TP}"
 -o "${arg_NAME}.h"
 -o "${arg_NAME}.c"
 "${CMAKE_CURRENT_SOURCE_DIR}/${arg_NAME}.tp"
 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
 DEPENDS "${arg_NAME}.tp"
 )
endfunction(add_lttng_gen_tp)