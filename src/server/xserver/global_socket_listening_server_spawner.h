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

#ifndef MIR_X_GLOBAL_SOCKET_LISTENING_SERVER_SPAWNER_H_
#define MIR_X_GLOBAL_SOCKET_LISTENING_SERVER_SPAWNER_H_

#include "mir/xserver/xserver_launcher.h"
#include "mir/process/spawner.h"
#include "mir/pipe.h"

#include <memory>

namespace mir
{
namespace X
{
class GlobalSocketListeningServerContext : public ServerContext
{
public:
    GlobalSocketListeningServerContext(std::shared_ptr<mir::process::Handle> server_handle, std::string connection_string);
    char const* client_connection_string() override;

private:
    std::shared_ptr<mir::process::Handle> server_handle;
    std::string connection_string;
};

class GlobalSocketListeningServerSpawner : public ServerSpawner
{
public:
    std::future<std::unique_ptr<ServerContext>> create_server(std::shared_ptr<mir::process::Spawner> const& spawner) override;
};

}
}

#endif // MIR_X_GLOBAL_SOCKET_LISTENING_SERVER_SPAWNER_H_
