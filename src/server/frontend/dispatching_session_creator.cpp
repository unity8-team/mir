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

#include "dispatching_session_creator.h"
#include "dispatched_session_creator.h"

mir::frontend::DispatchingSessionCreator::DispatchingSessionCreator(
    std::shared_ptr<std::vector<std::shared_ptr<mir::frontend::DispatchedSessionCreator>>> protocol_implementors,
    std::shared_ptr<mir::frontend::SessionAuthorizer> const& session_authorizer)
    : implementations(protocol_implementors), session_authorizer(session_authorizer)
{
}

void mir::frontend::DispatchingSessionCreator::create_session_for(
    std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket)
{
    auto header = std::make_shared<boost::asio::streambuf>();
    boost::asio::async_read(
        *socket,
        *header,
        boost::asio::transfer_exactly(36),
                [this, header, socket](boost::system::error_code const&, size_t) {
        uuid_t client_protocol_id;
        char client_protocol_str[37];
        std::istream{header.get()}.get(client_protocol_str, 37);
        uuid_parse(client_protocol_str, client_protocol_id);
        for (auto& protocol : *implementations)
        {
            uuid_t server_protocol_id;
            protocol->protocol_id(server_protocol_id);
            if (uuid_compare(client_protocol_id, server_protocol_id) == 0)
            {
                protocol->create_session_for(socket, "");
                return;
            }
        }
        BOOST_THROW_EXCEPTION(std::runtime_error(std::string("Unknown client protocol: ") + client_protocol_str));
    });
}
