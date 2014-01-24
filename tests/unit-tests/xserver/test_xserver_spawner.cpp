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

#include <future>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mir/process/spawner.h"

#include "src/server/xserver/global_socket_listening_server_spawner.h"

namespace
{
struct MockProcessSpawner : public mir::process::Spawner
{
    MOCK_CONST_METHOD3(run, std::shared_ptr<mir::process::Handle>(char const*, std::initializer_list<char const*>,
                                                                  std::initializer_list<int>));

    std::future<std::shared_ptr<mir::process::Handle>> run_from_path(char const* binary) const override
    {
        std::promise<std::shared_ptr<mir::process::Handle>> dummy_promise;
        dummy_promise.set_value(run(binary, std::initializer_list<char const*>(), std::initializer_list<int>()));
        return dummy_promise.get_future();
    }
    std::future<std::shared_ptr<mir::process::Handle>> run_from_path(char const* binary,
                                                                     std::initializer_list<char const*> args) const
        override
    {
        std::promise<std::shared_ptr<mir::process::Handle>> dummy_promise;
        dummy_promise.set_value(run(binary, args, std::initializer_list<int>()));
        return dummy_promise.get_future();
    }

    std::future<std::shared_ptr<mir::process::Handle>> run_from_path(char const* binary,
                                                                     std::initializer_list<char const*> args,
                                                                     std::initializer_list<int> fds) const override
    {
        std::promise<std::shared_ptr<mir::process::Handle>> dummy_promise;
        dummy_promise.set_value(run(binary, args, fds));
        return dummy_promise.get_future();
    }
};

struct SocketListeningServerTest : public testing::Test
{
    SocketListeningServerTest()
    {
        using namespace ::testing;
        ON_CALL(spawner, run(_, _, _)).WillByDefault(Return(std::shared_ptr<mir::process::Handle>()));
    }

    testing::NiceMock<MockProcessSpawner> spawner;
};
}

TEST_F(SocketListeningServerTest, CreateServerAlwaysValid)
{
    mir::X::GlobalSocketListeningServerSpawner factory;

    auto server_context = factory.create_server(spawner);

    ASSERT_NE(server_context, nullptr);
    EXPECT_NE(server_context->client_connection_string().get(), nullptr);
}

TEST_F(SocketListeningServerTest, SpawnsCorrectExecutable)
{
    using namespace ::testing;
    mir::X::GlobalSocketListeningServerSpawner factory;

    EXPECT_CALL(spawner, run(StrEq("Xorg"), _, _))
        .WillOnce(Return(std::shared_ptr<mir::process::Handle>()));

    auto server_context = factory.create_server(spawner);
    server_context->client_connection_string().get();
}

TEST_F(SocketListeningServerTest, ConnectionStringWaitsForServerStart)
{
    using namespace ::testing;
    mir::X::GlobalSocketListeningServerSpawner factory;

    auto server_context = factory.create_server(spawner);
    server_context->client_connection_string().get();
}
