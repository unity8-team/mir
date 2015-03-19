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

#include <future>
#include <functional>

#include <google/protobuf/service.h>

#include <gtest/gtest.h>

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
class RpcFutureResolver
{
public:
    RpcFutureResolver(std::future<std::unique_ptr<google::protobuf::RpcChannel>>&& future_channel)
        : future_channel{std::move(future_channel)}
    {
    }

    void set_completion(std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion)
    {
        completion(std::move(future_channel));
    }

private:
    std::future<std::unique_ptr<google::protobuf::RpcChannel>> future_channel;
};
}
}
}

namespace mclr = mir::client::rpc;

TEST(RpcFutureResolver, can_register_completed_callback)
{
    std::promise<std::unique_ptr<google::protobuf::RpcChannel>> promised_rpc;
    mclr::RpcFutureResolver resolver{promised_rpc.get_future()};

    resolver.set_completion([](std::future<std::unique_ptr<google::protobuf::RpcChannel>>) {});
}

TEST(RpcFutureResolver, completion_is_called_immediately_if_set_on_ready_resolver)
{
    std::promise<std::unique_ptr<google::protobuf::RpcChannel>> promised_rpc;
    promised_rpc.set_value({});

    mclr::RpcFutureResolver resolver{promised_rpc.get_future()};

    bool called{false};
    resolver.set_completion([&called](std::future<std::unique_ptr<google::protobuf::RpcChannel>>)
    {
        called = true;
    });
    EXPECT_TRUE(called);
}
