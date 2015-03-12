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
#include <endian.h>

#include "handshaking_connection_creator.h"
#include "protocol_interpreter.h"
#include "mir/frontend/connection_context.h"

namespace
{
struct ClientProtocolHeader
{
    template<typename Iterator>
    ClientProtocolHeader(std::array<char, 37> const& uuid_str, Iterator begin, Iterator end)
        : client_data(begin, end)
    {
        if (uuid_parse(uuid_str.data(), id) != 0)
        {
            using namespace std::literals::string_literals;
            BOOST_THROW_EXCEPTION((std::runtime_error{"Failed to parse UUID string "s + uuid_str.data()}));
        }
    }

    uuid_t id;
    std::vector<uint8_t> client_data;
};

std::vector<ClientProtocolHeader> parse_protocol_header(std::vector<uint8_t> const& wire_data)
{
    std::vector<ClientProtocolHeader> headers;

    auto read_pos = wire_data.cbegin();

    while (read_pos != wire_data.cend())
    {
        uint16_t client_header_size;
        std::array<char, 37> client_protocol_str;
        client_protocol_str[36] = '\0';


        client_header_size = le16toh(*reinterpret_cast<uint16_t const*>(&(*read_pos)));
        read_pos += 2;

        auto const id_end = read_pos + 36;
        auto const client_data_begin = id_end;
        auto const client_data_end = read_pos + client_header_size;

        std::copy(read_pos, id_end, client_protocol_str.begin());

        headers.emplace_back(client_protocol_str, client_data_begin, client_data_end);
        read_pos = client_data_end;
    }

    return headers;
}

}

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
    deadline->async_wait([socket](boost::system::error_code const& ec)
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
                            boost::asio::transfer_exactly(2),
                            [this, header, socket, deadline, connection_context](boost::system::error_code const&, size_t)
    {
        uint16_t total_header_size;
        std::istream header_size_data{header.get()};

        header_size_data.read(reinterpret_cast<char*>(&total_header_size), sizeof(uint16_t));
        total_header_size = le16toh(total_header_size);

        auto protocol_data_buffer = std::make_shared<std::vector<uint8_t>>(total_header_size);
        auto boost_buffer = boost::asio::buffer(*protocol_data_buffer);

        boost::asio::async_read(*socket,
                                boost_buffer,
                                boost::asio::transfer_exactly(total_header_size),
                                [this, protocol_data_buffer, socket, deadline, connection_context](boost::system::error_code const&, size_t)
        {
            deadline->cancel();

            auto protocols = parse_protocol_header(*protocol_data_buffer);

            for (auto& protocol : *implementations)
            {
                for (auto const& client_header : protocols)
                {
                    using namespace std::literals::string_literals;

                    uuid_t server_protocol_id;
                    auto& connection_protocol = protocol->connection_protocol();
                    connection_protocol.protocol_id(server_protocol_id);
                    if (uuid_compare(client_header.id, server_protocol_id) == 0)
                    {
                        std::array<char, 36> accepted_protocol_str;
                        uuid_unparse(client_header.id, accepted_protocol_str.data());

                        if (client_header.client_data.size() != connection_protocol.header_size())
                        {
                            BOOST_THROW_EXCEPTION((std::runtime_error{
                                "Client and server disagree on protocol header size for protocol "s +
                                accepted_protocol_str.data() + "! (expected: "s + std::to_string(connection_protocol.header_size()) +
                                " received: "s + std::to_string(client_header.client_data.size()) + ")"s}));
                        }

                        auto buf = boost::asio::buffer(accepted_protocol_str);
                        boost::asio::write(*socket, buf, boost::asio::transfer_all());

                        protocol->create_connection_for(socket, *session_authorizer, connection_context, "");
                        return;
                    }
                }
            }
            BOOST_THROW_EXCEPTION((std::runtime_error{"No matching protocols found"}));
        });

    });
}
