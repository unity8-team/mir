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


#include "mir/frontend/protobuf_handshake_protocol.h"

namespace mf = mir::frontend;
namespace mclr = mir::client::rpc;

void mf::ProtobufHandshakeProtocol::protocol_id(uuid_t id) const
{
    uuid_parse("60019143-2648-4904-9719-7817f0b9fb13", id);
}

size_t mf::ProtobufHandshakeProtocol::header_size() const
{
    return 0;
}

void mf::ProtobufHandshakeProtocol::write_client_header(uint8_t*) const
{
}

void mf::ProtobufHandshakeProtocol::send_server_header()
{
}

void mf::ProtobufHandshakeProtocol::receive_server_header(mclr::StreamTransport& /*transport*/)
{
    // We have no server header!
}
