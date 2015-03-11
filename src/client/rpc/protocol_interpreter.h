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

#ifndef MIR_CLIENT_RPC_PROTOCOL_INTERPRETER_H_
#define MIR_CLIENT_RPC_PROTOCOL_INTERPRETER_H_

#include <memory>
#include <google/protobuf/service.h>

namespace mir
{
namespace frontend
{
class HandshakeProtocol;
}

namespace client
{
namespace rpc
{
class StreamTransport;

class ProtocolInterpreter
{
public:
    ProtocolInterpreter() = default;
    virtual ~ProtocolInterpreter() = default;

    virtual std::unique_ptr<google::protobuf::RpcChannel> create_interpreter_for(
        std::unique_ptr<StreamTransport> transport) = 0;

    virtual frontend::HandshakeProtocol& connection_protocol() = 0;

private:
    ProtocolInterpreter(ProtocolInterpreter const&) = delete;
    ProtocolInterpreter& operator=(ProtocolInterpreter const&) = delete;
};
}
}
}

#endif  // MIR_CLIENT_RPC_PROTOCOL_INTERPRETER_H_
