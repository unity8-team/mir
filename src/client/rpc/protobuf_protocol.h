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

#ifndef MIR_CLIENT_RPC_PROTOBUF_PROTOCOL_H_
#define MIR_CLIENT_RPC_PROTOBUF_PROTOCOL_H_

#include "protocol_interpreter.h"
#include "mir/frontend/protobuf_handshake_protocol.h"

namespace mir
{
namespace client
{
class SurfaceMap;
class DisplayConfiguration;
class LifecycleControl;
class EventSink;

namespace rpc
{
class RpcReport;

class ProtobufProtocol : public ProtocolInterpreter
{
public:
    ProtobufProtocol(std::shared_ptr<SurfaceMap> const& map,
                     std::shared_ptr<DisplayConfiguration> const& disp_conf,
                     std::shared_ptr<RpcReport> const& rpc_report,
                     std::shared_ptr<LifecycleControl> const& lifecycle_control,
                     std::shared_ptr<EventSink> const& event_sink);
    
    std::unique_ptr<google::protobuf::RpcChannel> create_interpreter_for(std::unique_ptr<StreamTransport> transport) override;
    frontend::HandshakeProtocol& connection_protocol() override;
    
private:
    frontend::ProtobufHandshakeProtocol conn_proto;

    std::shared_ptr<SurfaceMap> const map;
    std::shared_ptr<DisplayConfiguration> const disp_conf;
    std::shared_ptr<RpcReport> const rpc_report;
    std::shared_ptr<LifecycleControl> const lifecycle_control;
    std::shared_ptr<EventSink> const event_sink;
};


}
}
}

#endif // MIR_CLIENT_RPC_PROTOBUF_PROTOCOL_H_
