/*
 * Copyright © 2013 Canonical Ltd.
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

#ifndef MIR_FRONTEND_PROTOBUF_SESSION_CREATOR_H_
#define MIR_FRONTEND_PROTOBUF_SESSION_CREATOR_H_

#include "dispatched_session_creator.h"
#include "mir/frontend/connected_sessions.h"

#include <atomic>

namespace mir
{
namespace protobuf { class DisplayServer; }
namespace frontend
{
class MessageProcessorReport;
class ProtobufIpcFactory;
class SessionAuthorizer;

namespace detail
{
struct SocketSession;
class MessageProcessor;
class ProtobufMessageSender;
}

class ProtobufSessionCreator : public DispatchedSessionCreator, public SessionCreator
{
public:
    ProtobufSessionCreator(
        std::shared_ptr<ProtobufIpcFactory> const& ipc_factory,
        std::shared_ptr<SessionAuthorizer> const& session_authorizer,
        std::shared_ptr<MessageProcessorReport> const& report);
    ~ProtobufSessionCreator() noexcept;

    void create_session_for(std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket) override;
    void create_session_for(std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket, const std::string &connection_data) override;

    void protocol_id(uuid_t id) const override;
    size_t header_size() const override;

    virtual std::shared_ptr<detail::MessageProcessor> create_processor(
        std::shared_ptr<detail::ProtobufMessageSender> const& sender,
        std::shared_ptr<protobuf::DisplayServer> const& display_server,
        std::shared_ptr<MessageProcessorReport> const& report) const;

private:
    int next_id();

    std::shared_ptr<ProtobufIpcFactory> const ipc_factory;
    std::shared_ptr<SessionAuthorizer> const session_authorizer;
    std::shared_ptr<MessageProcessorReport> const report;
    std::atomic<int> next_session_id;
    std::shared_ptr<detail::ConnectedSessions<detail::SocketSession>> const connected_sessions;

    // DispatchedSessionCreator interface
public:
};
}
}

#endif /* MIR_FRONTEND_PROTOBUF_SESSION_CREATOR_H_ */
