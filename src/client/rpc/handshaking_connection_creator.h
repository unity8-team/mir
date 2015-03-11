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

#ifndef MIR_CLIENT_RPC_HANDSHAKING_CONNECTION_CREATOR_H_
#define MIR_CLIENT_RPC_HANDSHAKING_CONNECTION_CREATOR_H_

#include <memory>
#include <vector>
#include <boost/thread/future.hpp>

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
class ProtocolInterpreter;
class StreamTransport;

class HandshakingConnectionCreator
{
public:
    HandshakingConnectionCreator(std::vector<std::unique_ptr<ProtocolInterpreter>>&& protocols);

    boost::future<std::unique_ptr<google::protobuf::RpcChannel>> connect_to(std::unique_ptr<StreamTransport> transport);

private:
    std::vector<std::unique_ptr<ProtocolInterpreter>> protocols;
    size_t total_header_size;
    std::vector<uint8_t> buffer;
};

}
}
}


#endif // MIR_CLIENT_RPC_HANDSHAKING_CONNECTION_CREATOR_H_
