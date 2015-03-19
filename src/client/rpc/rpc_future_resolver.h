/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_CLIENT_RPC_RPC_FUTURE_RESOLVER_H_
#define MIR_CLIENT_RPC_RPC_FUTURE_RESOLVER_H_

#include <future>
#include <thread>
#include <atomic>
#include <memory>
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
class RpcFutureResolver
{
public:
    RpcFutureResolver(std::future<std::unique_ptr<google::protobuf::RpcChannel>>&& future_channel);
    ~RpcFutureResolver();

    void set_completion(std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion);
    void cancel();

private:
    std::future<std::unique_ptr<google::protobuf::RpcChannel>> future_channel;
    std::thread wait_thread;
    std::atomic<bool> continue_wait;
};
}
}
}


#endif // MIR_CLIENT_RPC_RPC_FUTURE_RESOLVER_H_
