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

#include "src/server/xserver/global_socket_listening_server_spawner.h"

TEST(SocketListeningServerTest, CreateServerAlwaysValid)
{
    mir::X::GlobalSocketListeningServerSpawner factory;

    auto server_context = factory.create_server();
    ASSERT_NE(server_context, nullptr);
    EXPECT_NE(server_context->client_connection_string(), nullptr);
}
