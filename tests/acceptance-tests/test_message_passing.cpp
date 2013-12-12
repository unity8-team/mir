/*
 * Copyright © 2013 Canonical Ltd.
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

#include "mir_toolkit/mir_client_library.h"

#include "mir/frontend/connector.h"
#include "mir/display_server.h"
#include "mir/run_mir.h"

#include "mir_test_framework/testing_server_configuration.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace
{
// TODO we're only using part of TestingServerConfiguration - so that ought to be factored out
struct TestMessagePassingServerConfiguration : mir_test_framework::TestingServerConfiguration
{
private:
    using mir_test_framework::TestingServerConfiguration::exec;
    using mir_test_framework::TestingServerConfiguration::on_exit;
};

struct MessagePassing : testing::Test
{
    MessagePassing();
    ~MessagePassing();

    void SetUp();
    void TearDown();

    TestMessagePassingServerConfiguration server_config;
    std::string new_connection();

private:
    mir::DisplayServer* start_mir_server();

    std::thread server_thread;
    mir::DisplayServer* const display_server;
};

MessagePassing::MessagePassing() :
    server_config(),
    display_server{start_mir_server()}
{
}

void MessagePassing::SetUp()
{
    ASSERT_TRUE(display_server);
}

std::string MessagePassing::new_connection()
{
    char endpoint[128] = {0};
    sprintf(endpoint, "fd://%d", server_config.the_connector()->client_socket_fd());

    return endpoint;
}

void MessagePassing::TearDown()
{
    display_server->stop();
}

MessagePassing::~MessagePassing()
{
    if (server_thread.joinable()) server_thread.join();
}

mir::DisplayServer* MessagePassing::start_mir_server()
{
    std::mutex mutex;
    std::condition_variable cv;
    mir::DisplayServer* display_server{nullptr};

    server_thread = std::thread([&]
    {
        try
        {
            mir::run_mir(server_config, [&](mir::DisplayServer& ds)
            {
                std::unique_lock<std::mutex> lock(mutex);
                display_server = &ds;
                cv.notify_one();
            });
        }
        catch (std::exception const& e)
        {
            FAIL() << e.what();
        }
    });

    using namespace std::chrono;
    auto const time_limit = system_clock::now() + seconds(2);

    std::unique_lock<std::mutex> lock(mutex);

    while (!display_server && time_limit > system_clock::now())
        cv.wait_until(lock, time_limit);

    return display_server;
}

void test_surface_message_callback(
    MirSurface* /*surface*/,
    void* /*client_context*/,
    size_t /*size*/,
    void const* /*data*/)
{

}
}

MirSurface *mir_connection_create_unrealized_surface_sync(
    MirConnection */*connection*/,
    MirSurfaceParameters const */*params*/,
    mir_surface_message_callback /*message_callback*/,
    void */*message_context*/)
{
    // TODO implement and put in right place
    return nullptr;
}

void mir_surface_realize_sync(MirSurface */*surface*/)
{
    // TODO implement and put in right place
}



TEST_F(MessagePassing, create_and_realize_surface)
{
    auto const connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    ASSERT_TRUE(mir_connection_is_valid(connection));

    MirSurfaceParameters params;

    auto const surface = mir_connection_create_unrealized_surface_sync(
        connection,
        &params,
        test_surface_message_callback,
        this);

    // TODO what *should* this return for an unrealized surface?
//    EXPECT_TRUE(mir_surface_is_valid(surface));

    mir_surface_realize_sync(surface);

    // TODO this should pass
//    EXPECT_TRUE(mir_surface_is_valid(surface));

    // TODO code
//    mir_surface_release_sync(surface);

    mir_connection_release(connection);
}
