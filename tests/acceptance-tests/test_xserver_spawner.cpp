/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "src/server/xserver/global_socket_listening_server_spawner.h"
#include "mir_test_framework/testing_server_configuration.h"
#include "mir_test_framework/in_process_server.h"
#include "mir/xserver/xserver_launcher.h"
#include "src/server/process/fork_spawner.h"

#include <X11/Xlib.h>
#include <stdlib.h>

#include <gtest/gtest.h>

namespace mtf = mir_test_framework;
namespace mx = mir::X;

namespace
{
struct XserverSpawningServer : public mtf::InProcessServer
{
public:
    std::shared_ptr<mx::ServerSpawner> the_xserver_spawner()
    {
        return config.the_xserver_spawner();
    }

private:
    mir::DefaultServerConfiguration& server_config() override
    {
        return config;
    }

    class SocketListeningXServerConfig : public mtf::StubbedServerConfiguration
    {
    public:
        std::shared_ptr<mx::ServerSpawner> the_xserver_spawner() override
        {
            return std::make_shared<mx::GlobalSocketListeningServerSpawner> ();
        }
    } config;
};
}

TEST_F(XserverSpawningServer, X11ClientConnects)
{
    // Ensure the surrounding environment doesn't mess with the test
    unsetenv("DISPLAY");

    auto xserver = the_xserver_spawner()->create_server(std::make_shared<mir::process::ForkSpawner>());
    Display* disp = XOpenDisplay(xserver.get()->client_connection_string());

    ASSERT_TRUE(disp != NULL);

    XCloseDisplay(disp);
}
