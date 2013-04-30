# Add more folders to ship with the application, here
folder_01.source = qml/mirqt
folder_01.target = qml
DEPLOYMENTFOLDERS = folder_01

QMAKE_LFLAGS += "-L/usr/local/lib -L/usr/lib -lubuntu_application_api_mirserver"
QMAKE_CXXFLAGS += "-std=c++11 -I/usr/local/include/mir -g"

# Additional import path used to resolve QML modules in Creator's code model
QML_IMPORT_PATH =

# If your application uses the Qt Mobility libraries, uncomment the following
# lines and add the respective components to the MOBILITY variable.
# CONFIG += mobility
# MOBILITY +=

SOURCES += demo_shell.cpp application_switcher.cpp fullscreen_placement_strategy.cpp gl_cursor_renderer.cpp software_cursor_overlay_renderer.cpp

LIBS += -lubuntu_application_api_mirserver

# Please do not modify the following two lines. Required for deployment.
include(qtquick2applicationviewer/qtquick2applicationviewer.pri)
qtcAddDeployment()

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += mirserver
