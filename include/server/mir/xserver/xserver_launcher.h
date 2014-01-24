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

#include <memory>
#include <future>

#include "mir/process/spawner.h"

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
     *
     * \note The server will be ready to accept connections. The server will
     * be started, and waited for, if it is not already running.
     */
    virtual std::shared_future<std::string> client_connection_string() = 0;
};

class ServerSpawner
{
public:
    virtual ~ServerSpawner() = default;

    virtual std::unique_ptr<ServerContext> create_server(mir::process::Spawner const& spawner) = 0;
};
}
}

#endif // MIR_X_XSERVER_LAUNCHER_H_
