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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "src/server/frontend/socket_messenger.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace mir::frontend::detail;
using namespace boost::asio;

TEST(SocketMessengerTest, write_failures_never_throw)
{
    io_service svc;
    auto sock = std::make_shared<local::stream_protocol::socket>(svc);
    SocketMessenger mess(sock);

    EXPECT_NO_THROW( mess.send("foo") );
}
