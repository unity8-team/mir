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

#ifndef MIR_FRONTEND_PROTOBUF_HANDSHAKE_PROTOCOL_H_
#define MIR_FRONTEND_PROTOBUF_HANDSHAKE_PROTOCOL_H_

#include "mir/frontend/handshake_protocol.h"

namespace mir
{
namespace frontend
{
class ProtobufHandshakeProtocol : public HandshakeProtocol
{
public:
    void protocol_id(uuid_t id) const override;

    size_t header_size() const override;
    void send_client_header() override;

    void send_server_header() override;
    void receive_server_header() override;
};

}
}

#endif // MIR_FRONTEND_PROTOBUF_HANDSHAKE_PROTOCOL_H_
