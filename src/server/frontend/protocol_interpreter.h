/*
 * Copyright © 2015 Canonical Ltd.
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

#ifndef MIR_FRONTEND_PROTOCOL_INTERPRETER_H_
#define MIR_FRONTEND_PROTOCOL_INTERPRETER_H_

#include <memory>
#include <vector>
#include <boost/asio.hpp>

#include "mir/frontend/handshake_protocol.h"

namespace mir
{
namespace frontend
{
class SessionAuthorizer;
class ConnectionContext;
class HandshakeProtocol;

class ProtocolInterpreter
{
public:
    ProtocolInterpreter() = default;
    virtual ~ProtocolInterpreter() = default;

    virtual void create_connection_for(std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket,
                                       SessionAuthorizer& authorizer,
                                       ConnectionContext const& connection_context,
                                       std::string const& connection_data) = 0;

    virtual HandshakeProtocol& connection_protocol() = 0;

private:
    ProtocolInterpreter(ProtocolInterpreter const&) = delete;
    ProtocolInterpreter& operator=(ProtocolInterpreter const&) = delete;
};
}
}

#endif  // MIR_FRONTEND_PROTOCOL_INTERPRETER_H_
