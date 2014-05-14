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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_FRONTEND_CONNECTION_CONTEXT_H_
#define MIR_FRONTEND_CONNECTION_CONTEXT_H_

#include <functional>
#include <memory>

namespace mir
{
namespace frontend
{
class Connector;
class Session;

class ConnectionContext
{
public:
    ConnectionContext(Connector const* connector) :
        ConnectionContext([](std::shared_ptr<Session> const&){}, connector) {}
    ConnectionContext(
        std::function<void(std::shared_ptr<Session> const& session)> const connect_handler,
        Connector const* connector);

    int fd_for_new_client(std::function<void(std::shared_ptr<Session> const& session)> const& connect_handler) const;

    void handle_client_connect(std::shared_ptr<Session> const& session) const { connect_handler(session); }

private:
    std::function<void(std::shared_ptr<Session> const& session)> const connect_handler;
    Connector const* const connector;
};
}
}

#endif /* MIR_FRONTEND_CONNECTION_CONTEXT_H_ */
