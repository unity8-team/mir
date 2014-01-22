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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mir/process/spawner.h"

#include "src/server/xserver/global_socket_listening_server_spawner.h"

namespace
{
struct MockProcessSpawner : public mir::process::Spawner
{
    MOCK_CONST_METHOD1(run_from_path, std::shared_ptr<mir::process::Handle>(char const*));
    MOCK_CONST_METHOD2(run_from_path, std::shared_ptr<mir::process::Handle>(char const*, std::initializer_list<char const*>));
};

struct SocketListeningServerTest : public testing::Test
{
    SocketListeningServerTest()
    {
        using namespace ::testing;
        ON_CALL(spawner, run_from_path(_,_))
            .WillByDefault(Return(std::shared_ptr<mir::process::Handle>()));
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

    EXPECT_CALL(spawner, run_from_path(StrEq("Xorg"), _))
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
