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

#ifndef MIR_CLIENT_RPC_RPC_CHANNEL_RESOLVER_H_
#define MIR_CLIENT_RPC_RPC_CHANNEL_RESOLVER_H_

#include <functional>
#include <memory>
#include <future>

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
    virtual ~RpcChannelResolver() = default;
    virtual void set_completion(
        std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion) = 0;
    virtual std::unique_ptr<google::protobuf::RpcChannel> get() = 0;
};
}
}
}

#endif  // MIR_CLIENT_RPC_RPC_CHANNEL_RESOLVER_H_
