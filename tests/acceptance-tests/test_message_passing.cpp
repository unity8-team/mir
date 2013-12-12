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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mir/default_server_configuration.h"
#include "mir/display_server.h"
#include "mir/run_mir.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace
{
struct TestMessagePassingServerConfiguration : mir::DefaultServerConfiguration
{
    using mir::DefaultServerConfiguration::DefaultServerConfiguration;
};

struct MessagePassingServerTestFixture : testing::Test
{
    MessagePassingServerTestFixture();
    ~MessagePassingServerTestFixture();
//
//    static void SetUpTestCase();
//    static void TearDownTestCase();
//
//    void SetUp();
//    void TearDown();

    TestMessagePassingServerConfiguration server_config;

    mir::DisplayServer* run_mir_server();

private:
    std::thread server_thread;
};

MessagePassingServerTestFixture::MessagePassingServerTestFixture() :
    server_config(0, nullptr)
{
}

MessagePassingServerTestFixture::~MessagePassingServerTestFixture()
{
    if (server_thread.joinable()) server_thread.join();
}

mir::DisplayServer* MessagePassingServerTestFixture::run_mir_server()
{
    std::mutex mutex;
    std::condition_variable cv;
    mir::DisplayServer* display_server{nullptr};

    server_thread = std::thread([&]
    {
        ASSERT_NO_THROW(
        mir::run_mir(server_config,
            [&] (mir::DisplayServer& ds)
            {
                std::unique_lock<std::mutex> lock(mutex);
                display_server = &ds;
                cv.notify_one();
            }));
    });

    using namespace std::chrono;
    auto const time_limit = system_clock::now() + seconds(2);

    std::unique_lock<std::mutex> lock(mutex);
    while (!display_server && time_limit > system_clock::now())
        cv.wait_until(lock, time_limit);

    return display_server;
}
}

TEST_F(MessagePassingServerTestFixture, try_running_test_in_process)
{
    auto const display_server = run_mir_server();
    ASSERT_TRUE(display_server);
    display_server->stop();
}
