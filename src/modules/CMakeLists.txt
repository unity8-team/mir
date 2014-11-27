include(QmlPlugins)

# Set up and install a QML plugin.
#
# add_qml_plugin(plugin version path
#     [...]  # See export_qmlfiles and export_qmlplugin documentation for more options
# )

macro(add_qml_plugin PLUGIN VERSION PATH)
    export_qmlfiles(${PLUGIN} ${PATH} DESTINATION ${QML_MODULE_INSTALL_DIR} ${ARGN})
    export_qmlplugin(${PLUGIN} ${VERSION} ${PATH} DESTINATION ${QML_MODULE_INSTALL_DIR} ${ARGN})
endmacro()

add_subdirectory(Unity)
