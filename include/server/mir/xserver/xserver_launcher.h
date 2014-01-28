/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_X_XSERVER_LAUNCHER_H_
#define MIR_X_XSERVER_LAUNCHER_H_

#include <future>
#include <memory>

#include "mir/process/spawner.h"
#include "mir/frontend/connector.h"

namespace mir
{
namespace X
{

class ServerContext
{
public:
    virtual ~ServerContext() = default;

    /**
     * \brief Get the XLib connection string to connect to this server
     *
     * This string can be passed by the client to XOpenDisplay to connect
     * to this server instance, or set in the DISPLAY environment variable
     * to be used as the default display.
     */
    virtual char const* client_connection_string() = 0;
};

class ServerSpawner
{
public:
    virtual ~ServerSpawner() = default;

    virtual std::future<std::unique_ptr<ServerContext>> create_server(std::shared_ptr<mir::process::Spawner> const& spawner,
                                                                      std::shared_ptr<mir::frontend::Connector> const& connector) = 0;
};
}
}

#endif // MIR_X_XSERVER_LAUNCHER_H_
