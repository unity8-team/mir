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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 *              Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_framework/detect_server.h"

#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mf = mir::frontend;
namespace mc = mir::compositor;
namespace mtf = mir_test_framework;

namespace mir
{
TEST_F(BespokeDisplayServerTestFixture, server_announces_itself_on_startup)
{
    ASSERT_FALSE(mtf::detect_server(mtf::test_socket_file(), std::chrono::milliseconds(0)));

    TestingServerConfiguration server_config;

    launch_server_process(server_config);

    struct ClientConfig : TestingClientConfiguration
    {
        void exec()
        {
            EXPECT_TRUE(mtf::detect_server(mtf::test_socket_file(),
                                           std::chrono::milliseconds(100)));
        }
    } client_config;

    launch_client_process(client_config);
}

TEST_F(BespokeDisplayServerTestFixture, can_start_new_instance_after_sigkill)
{
    ASSERT_FALSE(mtf::detect_server(mtf::test_socket_file(), std::chrono::milliseconds(0)));

    TestingServerConfiguration config;
    launch_server_process(config);

    run_in_test_process([&]
    {
        /* Under valgrind, raise(SIGKILL) results in a memcheck orphan process
         * we kill the server process from the test process instead
         */
        EXPECT_TRUE(kill_server_process());

        /* Attempt to start a new server instance */
        TestingServerConfiguration server_config;
        launch_server_process(server_config);

        struct ClientConfig : TestingClientConfiguration
        {
            void exec()
            {
                EXPECT_TRUE(mtf::detect_server(mtf::test_socket_file(),
                                               std::chrono::milliseconds(100)));
            }
        } client_config;

        launch_client_process(client_config);
    });
}
}
