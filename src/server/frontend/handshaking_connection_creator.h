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


#ifndef MIR_FRONTEND_HANDSHAKING_SESSION_CREATOR_H_
#define MIR_FRONTEND_HANDSHAKING_SESSION_CREATOR_H_

#include <memory>
#include <vector>

#include "mir/frontend/session_authorizer.h"
#include "mir/frontend/connection_creator.h"

namespace mir
{
namespace frontend
{
class ProtocolInterpreter;

/**
 * \brief Perform client connection handshake and dispatch to the appropriate protocol implementation.
 *
 * Theory of operation:
 *
 * On connection to the socket, the client sends the total connection data size, plus
 * one or more sets of {size, UUID, extra_data} describing the protocols it supports
 * and any extra data that those protocols need. Following capnproto's lead, all structures
 * on the wire are little-endian.
 *
 * In total, the connection packet looks like:
 * uint16_t length - subsequent connection data, in bytes
 *   [
 *     uint16_t length - of subsequent protocol data packet, in bytes
 *     36 bytes of UUID in string form, without terminating null byte (eg: "60019143-2648-4904-9719-7817f0b9fb13")
 *     (length - 36) bytes of opaque data, passed to protocol implementation
 *   ]…
 *
 * This gives the server enough information to skip any protocols it does not support.
 *
 * The HandshakingConnectionCreator finds the first ProtocolInterpreter with a
 * HandshakeProtocol that matches one of the client-supported UUIDs then calls
 * its create_connnection_for method with the extra_data from the client connect packet.
 */
class HandshakingConnectionCreator : public ConnectionCreator
{
public:
    HandshakingConnectionCreator(std::shared_ptr<std::vector<std::shared_ptr<ProtocolInterpreter>>> protocol_implementors,
                                 std::shared_ptr<SessionAuthorizer> const& session_authorizer);

    void create_connection_for(std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket,
                               ConnectionContext const& connection_context) override;
private:
    std::shared_ptr<std::vector<std::shared_ptr<ProtocolInterpreter>>> const implementations;
    std::shared_ptr<SessionAuthorizer> const session_authorizer;
};
}
}

#endif // MIR_FRONTEND_HANDSHAKING_SESSION_CREATOR_H_
