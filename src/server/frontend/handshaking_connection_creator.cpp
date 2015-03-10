/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Christopher Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include <uuid/uuid.h>
#include <istream>
#include <boost/throw_exception.hpp>
#include <string>

#include "handshaking_connection_creator.h"
#include "protocol_interpreter.h"
#include "mir/frontend/connection_context.h"

mir::frontend::HandshakingConnectionCreator::HandshakingConnectionCreator(
    std::shared_ptr<std::vector<std::shared_ptr<mir::frontend::ProtocolInterpreter>>> protocol_implementors,
    std::shared_ptr<mir::frontend::SessionAuthorizer> const& session_authorizer)
    : implementations(protocol_implementors), session_authorizer(session_authorizer)
{
}

void mir::frontend::HandshakingConnectionCreator::create_connection_for(std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket, const ConnectionContext &connection_context)
{
    auto header = std::make_shared<boost::asio::streambuf>();

    auto deadline = std::make_shared<boost::asio::deadline_timer>(socket->get_io_service());
    // We're all local systems here; 500ms from socket connect to header write seems generous.
    deadline->expires_from_now(boost::posix_time::milliseconds{500});
    deadline->async_wait([&socket](boost::system::error_code const& ec)
                         {
                             if (!ec)
                             {
                                 socket->cancel();
                                 BOOST_THROW_EXCEPTION(
                                     std::runtime_error("Timeout waiting for client to send connection header"));
                             }
                         });

    boost::asio::async_read(*socket,
                            *header,
                            boost::asio::transfer_exactly(sizeof(uint16_t) + 36),
                            [this, header, socket, deadline, connection_context](boost::system::error_code const&, size_t)
                            {
        deadline->cancel();

        uint16_t client_header_size;
        uuid_t client_protocol_id;
        char client_protocol_str[36 + 1];
        client_protocol_str[36] = '\0';

        std::istream header_data{header.get()};

        header_data.read(reinterpret_cast<char*>(&client_header_size), sizeof(uint16_t));
        client_header_size = ntohs(client_header_size);

        header_data.read(client_protocol_str, 36);
        uuid_parse(client_protocol_str, client_protocol_id);

        for (auto& protocol : *implementations)
        {
            using namespace std::literals::string_literals;
            uuid_t server_protocol_id;
            auto& connection_protocol = protocol->connection_protocol();
            connection_protocol.protocol_id(server_protocol_id);
            if (uuid_compare(client_protocol_id, server_protocol_id) == 0)
            {
                if (client_header_size != connection_protocol.header_size())
                    BOOST_THROW_EXCEPTION((std::runtime_error{
                        "Client and server disagree on protocol header size for protocol "s +
                        client_protocol_str + "! (expected: "s + std::to_string(connection_protocol.header_size()) +
                        " received: "s + std::to_string(client_header_size) + ")"s}));

                protocol->create_connection_for(socket, *session_authorizer, connection_context, "");
                return;
            }
        }
        BOOST_THROW_EXCEPTION(std::runtime_error(std::string("Unknown client protocol: ") + client_protocol_str));
    });
}
