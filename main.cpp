/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include <QtGui/QGuiApplication>
#include "qtquick2applicationviewer.h"

#include <mirserver/mir/run_mir.h>
#include <mirserver/mir/default_server_configuration.h>
#include <mirserver/mir/abnormal_exit.h>

#include <thread>

#include <boost/exception/diagnostic_information.hpp>

// TODO: install this header rather than copy...hack anyway
#include "ubuntu_application_api_mirserver.h"


namespace
{

void run_qml_shell()
{
    const char *argv[] = {"hellomir", "-platform", "ubuntu", NULL};
    int argc = 3;
    QGuiApplication app(argc, (char **)argv);

    QtQuick2ApplicationViewer viewer;
    viewer.setMainQmlFile(QStringLiteral("qml/mirqt/Launcher.qml"));
    viewer.showExpanded();

    app.exec();
}

void run_shell()
{
//    demo_shell_ds = display_server;
//    ubuntu_application_api_mir_set_display_server(display_server);
    (new std::thread(run_qml_shell));
}
}

int main(int argc, char const* argv[])
try
{
    mir::DefaultServerConfiguration config(argc, argv);

    mir::run_mir(config, [&config](mir::DisplayServer&) {
        ubuntu_application_ui_mirserver_init(config.the_session_manager(),
                                                 config.the_graphics_platform(),
                                                 config.the_display());
        run_shell();
    });
    return 0;
}
catch (mir::AbnormalExit const& error)
{
    std::cerr << error.what() << std::endl;
    return 1;
}
catch (std::exception const& error)
{
    std::cerr << "ERROR: " << boost::diagnostic_information(error) << std::endl;
    return 1;
}

