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

#include "protobuf_session_creator.h"

#include "event_sender.h"
#include "protobuf_message_processor.h"
#include "socket_messenger.h"
#include "socket_session.h"

#include "mir/frontend/protobuf_ipc_factory.h"
#include "mir/frontend/session_authorizer.h"
#include "mir/protobuf/google_protobuf_guard.h"

namespace mf = mir::frontend;
namespace mfd = mir::frontend::detail;
namespace ba = boost::asio;

mf::ProtobufSessionCreator::ProtobufSessionCreator(
    std::shared_ptr<ProtobufIpcFactory> const& ipc_factory,
    std::shared_ptr<mf::SessionAuthorizer> const& session_authorizer)
:   ipc_factory(ipc_factory),
    session_authorizer(session_authorizer),
    next_session_id(0),
    connected_sessions(std::make_shared<mfd::ConnectedSessions<mfd::SocketSession>>())
{
}

mf::ProtobufSessionCreator::~ProtobufSessionCreator() noexcept
{
    connected_sessions->clear();
}

int mf::ProtobufSessionCreator::next_id()
{
    return next_session_id.fetch_add(1);
}

void mf::ProtobufSessionCreator::create_session_for(std::shared_ptr<ba::local::stream_protocol::socket> const& socket)
{
    auto const messenger = std::make_shared<detail::SocketMessenger>(socket, ipc_factory->messenger_report());
    auto const client_pid = messenger->client_pid();

    if (session_authorizer->connection_is_allowed(client_pid))
    {
        auto const authorized_to_resize_display = session_authorizer->configure_display_is_allowed(client_pid);
        auto const event_sink = std::make_shared<detail::EventSender>(messenger);
        auto const msg_processor = std::make_shared<detail::ProtobufMessageProcessor>(
            messenger,
            ipc_factory->make_ipc_server(event_sink, authorized_to_resize_display),
            ipc_factory->resource_cache(),
            ipc_factory->message_processor_report());

        const auto& session = std::make_shared<mfd::SocketSession>(messenger, next_id(), connected_sessions, msg_processor);
        connected_sessions->add(session);
        session->read_next_message();
    }
}
