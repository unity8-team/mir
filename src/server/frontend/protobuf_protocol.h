/*
 * Copyright © 2013-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_FRONTEND_PROTOBUF_PROTOCOL_H_
#define MIR_FRONTEND_PROTOBUF_PROTOCOL_H_

#include "protocol_interpreter.h"
#include "mir/frontend/connections.h"
#include "mir/frontend/protobuf_handshake_protocol.h"

#include <atomic>

namespace mir
{
namespace frontend
{
class MessageProcessorReport;
class ProtobufIpcFactory;
class SessionAuthorizer;

namespace detail
{
class DisplayServer;
class SocketConnection;
class MessageProcessor;
class ProtobufMessageSender;
}

class ProtobufProtocol : public ProtocolInterpreter
{
public:
    ProtobufProtocol(std::unique_ptr<ProtobufIpcFactory> ipc_factory,
                     std::shared_ptr<MessageProcessorReport> const& report);
    ~ProtobufProtocol() noexcept;

    void create_connection_for(std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket,
                               SessionAuthorizer& authorizer,
                               ConnectionContext const& connection_context,
                               std::string const& connection_data) override;

    HandshakeProtocol& connection_protocol() override;

    virtual std::shared_ptr<detail::MessageProcessor> create_processor(
        std::shared_ptr<detail::ProtobufMessageSender> const& sender,
        std::shared_ptr<detail::DisplayServer> const& display_server,
        std::shared_ptr<MessageProcessorReport> const& report) const;

private:
    int next_id();

    ProtobufHandshakeProtocol connect_proto;
    std::unique_ptr<ProtobufIpcFactory> const ipc_factory;
    std::shared_ptr<MessageProcessorReport> const report;
    std::atomic<int> next_session_id;

    std::shared_ptr<detail::Connections<detail::SocketConnection>> const connections;
};
}
}

#endif /* MIR_FRONTEND_PROTOBUF_PROTOCOL_H_ */
