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

#include "protobuf_protocol.h"
#include "stream_transport.h"
#include "mir_protobuf_rpc_channel.h"
#include "../connection_configuration.h"

namespace mcl = mir::client;
namespace mclr = mir::client::rpc;

mclr::ProtobufProtocol::ProtobufProtocol(std::shared_ptr<mcl::SurfaceMap> const& map,
                                         std::shared_ptr<mcl::DisplayConfiguration> const& disp_conf,
                                         std::shared_ptr<mclr::RpcReport> const& rpc_report,
                                         std::shared_ptr<mcl::LifecycleControl> const& lifecycle_control,
                                         std::shared_ptr<mcl::EventSink> const& event_sink)
    : map{map},
      disp_conf{disp_conf},
      rpc_report{rpc_report},
      lifecycle_control{lifecycle_control},
      event_sink{event_sink}
{
}


std::unique_ptr<google::protobuf::RpcChannel> mclr::ProtobufProtocol::create_interpreter_for(
    std::unique_ptr<mclr::StreamTransport> transport)
{
    return std::make_unique<MirProtobufRpcChannel>(std::move(transport), map, disp_conf, rpc_report, lifecycle_control, event_sink);
}

mir::frontend::HandshakeProtocol& mclr::ProtobufProtocol::connection_protocol()
{
    return conn_proto;
}
