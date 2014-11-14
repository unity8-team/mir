CONFIG += no_keywords # keywords clash with ProcessC++
PKGCONFIG += ubuntu-app-launch-2 process-cpp

QT += quick

HEADERS += ../common/mock_application_controller.h \
    ../common/mock_desktop_file_reader.h \
    ../common/mock_focus_controller.h \
    ../common/mock_mir_session.h \
    ../common/mock_proc_info.h \
    ../common/mock_prompt_session.h \
    ../common/mock_prompt_session_manager.h \
    ../common/mock_renderable.h \
    ../common/mock_session.h \
    ../common/mock_surface.h \
    ../common/qtmir_test.h \
    ../common/stub_input_channel.h \
    ../common/stub_scene_surface.h

INCLUDEPATH += ../../../src/modules \
    ../common

LIBS += \
    -Wl,-rpath,$${OUT_PWD}/../../../src/modules/Unity/Application \
    -L$${OUT_PWD}/../../../src/modules/Unity/Application -lunityapplicationplugin \
    -Wl,-rpath,$${OUT_PWD}/../../../src/platforms/mirserver \
    -L$${OUT_PWD}/../../../src/platforms/mirserver
