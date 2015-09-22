add_definitions(-DMIR_LOG_COMPONENT_FALLBACK="mirplatform-input")

include_directories(
    ${PROJECT_SOURCE_DIR}/include/platform  # module properties
    ${PROJECT_SOURCE_DIR}/include/platform-input
    ${PROJECT_SOURCE_DIR}/include/client
    ${PROJECT_SOURCE_DIR}/src/include/platform-input
    )

set(MIRPLATFORM_INPUT_ABI 0)
set(symbol_map ${CMAKE_CURRENT_SOURCE_DIR}/symbols.map)

add_subdirectory(input)

add_library(mirplatform-input SHARED
    $<TARGET_OBJECTS:mirplatforminput>
    )

set_target_properties(
    mirplatform-input

    PROPERTIES
    SOVERSION ${MIRPLATFORM_INPUT_ABI}
    LINK_FLAGS "-Wl,--exclude-libs=ALL -Wl,--version-script,${symbol_map}"
    )

install(TARGETS mirplatform-input LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# pkg-config setup:
set(INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include/mirplatform-input")
set(COMMON_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include/mircommon") # utilities
set(CLIENT_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include/mirclient") # typedefs 
set(PLATFORM_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include/mirplatform") # module properties
set(LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/mirplatform-input.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mirplatform-input.pc
    )

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/mirplatform-input.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
