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

#ifndef MIR_FRONTEND_HANDSHAKE_PROTOCOL_H_
#define MIR_FRONTEND_HANDSHAKE_PROTOCOL_H_

#include <memory>
#include <vector>
#include <uuid/uuid.h>

namespace mir
{
namespace frontend
{
class SessionAuthorizer;

/**
 * \brief A descriptor of the initial socket handshake needed to connect a protocol
 *
 */
class HandshakeProtocol
{
public:
    HandshakeProtocol() = default;
    virtual ~HandshakeProtocol() = default;

    /**
     * \brief The UUID of this protocol
     * \param [out] id  The protocol's uuid.
     */
    virtual void protocol_id(uuid_t id) const = 0;

    /**
     * \brief The size, in bytes, of extra data sent along with the UUID.
     */
    virtual size_t header_size() const = 0;
    /**
     * \brief Send the extra client connection data for this protocol.
     *
     * \note receive_client_header is unnecessary; the header_size is sent on client connect
     */
    virtual void send_client_header() = 0;

    /**
     * \brief Send the server connection reply
     */
    virtual void send_server_header() = 0;
    /**
     * \brief Read the server connection reply
     */
    virtual void receive_server_header() = 0;

private:
    HandshakeProtocol(HandshakeProtocol const&) = delete;
    HandshakeProtocol& operator=(HandshakeProtocol const&) = delete;
};
}
}

#endif  // MIR_FRONTEND_CONNECTION_PROTOCOL_H_
