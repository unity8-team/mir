/*
 * Copyright © 2013-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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
 */

#include "mir_test_framework/server_runner.h"

#include "mir/default_server_configuration.h"
#include "mir/display_server.h"
#include "mir/frontend/connector.h"
#include "mir/run_mir.h"

#include <gtest/gtest.h>

#include <condition_variable>
#include <mutex>

namespace mtf = mir_test_framework;

namespace
{
char const* const env_no_file = "MIR_SERVER_NO_FILE";
}

mtf::ServerRunner::ServerRunner() :
    old_env(getenv(env_no_file))
{
    if (!old_env) setenv(env_no_file, "", true);
}

void mtf::ServerRunner::start_server()
{
    display_server = start_mir_server();
    ASSERT_TRUE(display_server);
}

std::string mtf::ServerRunner::new_connection()
{
    char connect_string[64] = {0};
    sprintf(connect_string, "fd://%d", server_config().the_connector()->client_socket_fd());
    return connect_string;
}

std::string mtf::ServerRunner::new_prompt_connection()
{
    char connect_string[64] = {0};
    sprintf(connect_string, "fd://%d", server_config().the_prompt_connector()->client_socket_fd());
    return connect_string;
}

void mtf::ServerRunner::stop_server()
{
    ASSERT_TRUE(display_server);
    display_server->stop();
}

mtf::ServerRunner::~ServerRunner()
{
    if (server_thread.joinable()) server_thread.join();

    if (!old_env) unsetenv(env_no_file);
}

mir::DisplayServer* mtf::ServerRunner::start_mir_server()
{
    std::mutex mutex;
    std::condition_variable started;
    mir::DisplayServer* result{nullptr};

    server_thread = std::thread([&]
    {
        try
        {
            mir::run_mir(server_config(), [&](mir::DisplayServer& ds)
            {
                std::lock_guard<std::mutex> lock(mutex);
                result = &ds;
                started.notify_one();
            });
        }
        catch (std::exception const& e)
        {
            FAIL() << e.what();
        }
    });

    std::unique_lock<std::mutex> lock(mutex);
    started.wait_for(lock, std::chrono::seconds{10}, [&]{ return !!result; });

    return result;
}
