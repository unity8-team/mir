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

#include "mir/xserver/null_server_spawner.h"

mir::X::NullServerContext::NullServerContext()
{
    connection_string.set_value("");
}

std::shared_future<std::string> mir::X::NullServerContext::client_connection_string()
{
    return connection_string.get_future();
}

std::unique_ptr<mir::X::ServerContext> mir::X::NullServerSpawner::create_server(mir::process::Spawner const& unused)
{
    static_cast<void>(unused);
    return std::unique_ptr<mir::X::ServerContext> (new mir::X::NullServerContext);
}
