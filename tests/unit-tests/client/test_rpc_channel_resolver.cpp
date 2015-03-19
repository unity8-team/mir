/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Christohper James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include <gtest/gtest.h>

#include <future>
#include <functional>

namespace google
{
namespace protobuf
{
class RpcChannel;
}
}

namespace mir
{
namespace client
{
namespace rpc
{
class RpcChannelResolver
{
public:
    void set_completion(std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> /*completion*/)
    {
    }
};
}
}
}

namespace mclr = mir::client::rpc;

TEST(RpcChannelResolver, can_register_completed_callback)
{
    mclr::RpcChannelResolver resolver;

    resolver.set_completion([](std::future<std::unique_ptr<google::protobuf::RpcChannel>>) {});
}
