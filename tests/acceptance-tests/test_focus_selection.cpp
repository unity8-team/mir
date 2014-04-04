/*
 * Copyright © 2013 Canonical Ltd.
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

#include "mir_toolkit/mir_client_library.h"

#include "src/server/scene/session_container.h"
#include "src/server/shell/consuming_placement_strategy.h"
#include "src/server/scene/session_manager.h"
#include "mir/graphics/display.h"
#include "mir/shell/input_targeter.h"

#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_doubles/mock_focus_setter.h"
#include "mir_test_doubles/mock_input_targeter.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace mi = mir::input;
namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace mt = mir::test;
namespace mtf = mir_test_framework;

namespace
{
    char const* const mir_test_socket = mtf::test_socket_file().c_str();
}

namespace
{

struct SurfaceCreatingClient : mtf::TestingClientConfiguration
{
    void exec()
    {
        connection = mir_connect_sync(
            mir_test_socket,
            __PRETTY_FUNCTION__);
         ASSERT_TRUE(connection != NULL);
         MirSurfaceParameters const request_params =
         {
            __PRETTY_FUNCTION__,
            640, 480,
            mir_pixel_format_abgr_8888,
            mir_buffer_usage_hardware,
            mir_display_output_id_invalid
         };
         surface = mir_connection_create_surface_sync(connection, &request_params);
         mir_connection_release(connection);
    }

    MirConnection* connection;
    MirSurface* surface;
};

}

namespace
{
MATCHER(NonNullSession, "")
{
    return arg != std::shared_ptr<msh::Session>();
}
}

TEST_F(BespokeDisplayServerTestFixture, sessions_creating_surface_receive_focus)
{
    struct ServerConfig : TestingServerConfiguration
    {
        std::shared_ptr<msh::FocusSetter>
        the_shell_focus_setter() override
        {
            return shell_focus_setter(
            []
            {
                using namespace ::testing;

                auto focus_setter = std::make_shared<mtd::MockFocusSetter>();
                {
                    InSequence seq;
                    // Once on application registration and once on surface creation
                    EXPECT_CALL(*focus_setter, set_focus_to(NonNullSession())).Times(2);
                    // Focus is cleared when the session is closed
                    EXPECT_CALL(*focus_setter, set_focus_to(_)).Times(1);
                }
                // TODO: Counterexample ~racarr

                return focus_setter;
            });
        }
    } server_config;

    launch_server_process(server_config);

    SurfaceCreatingClient client;

    launch_client_process(client);
}

TEST_F(BespokeDisplayServerTestFixture, surfaces_receive_input_focus_when_created)
{
    struct ServerConfig : TestingServerConfiguration
    {
        std::shared_ptr<mtd::MockInputTargeter> targeter;
        bool expected;

        ServerConfig()
          : targeter(std::make_shared<mtd::MockInputTargeter>()),
            expected(false)
        {
        }

        std::shared_ptr<msh::InputTargeter>
        the_input_targeter() override
        {
            using namespace ::testing;

            if (!expected)
            {

                EXPECT_CALL(*targeter, focus_cleared()).Times(AtLeast(0));

                {
                    InSequence seq;
                    EXPECT_CALL(*targeter, focus_changed(_)).Times(1);
                    expected = true;
                }
            }

            return targeter;
        }
    } server_config;


    launch_server_process(server_config);

    SurfaceCreatingClient client;

    launch_client_process(client);
}
