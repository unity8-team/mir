/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "make_rpc_channel.h"
#include "mir_protobuf_rpc_channel.h"
#include "mir/frontend/protobuf_handshake_protocol.h"
#include "stream_socket_transport.h"

#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

#include <cstring>

namespace mcl = mir::client;
namespace mf = mir::frontend;
namespace mclr = mir::client::rpc;

namespace
{
struct Prefix
{
    template<int Size>
    Prefix(char const (&prefix)[Size]) : size(Size-1), prefix(prefix) {}

    bool is_start_of(std::string const& name) const
    { return !strncmp(name.c_str(), prefix, size); }

    int const size;
    char const* const prefix;
} const fd_prefix("fd://");
}

std::shared_ptr<google::protobuf::RpcChannel>
mclr::make_rpc_channel(std::string const& name,
                       std::shared_ptr<mcl::SurfaceMap> const& map,
                       std::shared_ptr<mcl::DisplayConfiguration> const& disp_conf,
                       std::shared_ptr<RpcReport> const& rpc_report,
                       std::shared_ptr<mcl::LifecycleControl> const& lifecycle_control,
                       std::shared_ptr<mcl::EventSink> const& event_sink)
{
    std::unique_ptr<mclr::StreamTransport> transport;
    if (fd_prefix.is_start_of(name))
    {
        auto const fd = atoi(name.c_str()+fd_prefix.size);
        transport = std::make_unique<mclr::StreamSocketTransport>(mir::Fd{fd});
    }
    else
    {
        transport = std::make_unique<mclr::StreamSocketTransport>(name);
    }
    // TODO: Multiple co-existing client protocols, server replies.
    mf::ProtobufHandshakeProtocol handshake;
    std::vector<uint8_t> buffer(38);
    uuid_t id;
    handshake.protocol_id(id);
    *reinterpret_cast<uint16_t*>(buffer.data()) = handshake.header_size();
    uuid_unparse(id, reinterpret_cast<char*>(buffer.data()) + 2);

    transport->send_message(buffer, {});
    return std::make_shared<MirProtobufRpcChannel>(std::move(transport), map, disp_conf, rpc_report, lifecycle_control, event_sink);
}
