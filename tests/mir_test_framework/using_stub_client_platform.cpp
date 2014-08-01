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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir_test_framework/using_stub_client_platform.h"
#include "mir_test_framework/stub_client_connection_configuration.h"
#include "src/client/mir_wait_handle.h"
#include "src/client/mir_connection.h"
#include "src/client/api_impl.h"

namespace mtf = mir_test_framework;

namespace
{

void null_lifecycle_callback(MirConnection*, MirLifecycleState, void*)
{
}

MirWaitHandle* mir_connect_override(
    char const *socket_file,
    char const *app_name,
    mir_connected_callback callback,
    void *context)
{
    mtf::StubConnectionConfiguration conf(socket_file);
    auto connection = new MirConnection(conf);
    return connection->connect(app_name, callback, context);
}

void mir_connection_release_override(MirConnection *connection)
{
    try
    {
        mir_connection_set_lifecycle_event_callback(connection, null_lifecycle_callback, nullptr);
        auto wait_handle = connection->disconnect();
        mir_wait_for(wait_handle);
    }
    catch (std::exception const&)
    {
        // Really, we want try/finally, but that's not C++11
        delete connection;
        throw;
    }
    delete connection;
}

}

mtf::UsingStubClientPlatform::UsingStubClientPlatform()
    : prev_mir_connect_impl{mir_connect_impl},
      prev_mir_connection_release_impl{mir_connection_release_impl}
{
    mir_connect_impl = mir_connect_override;
    mir_connection_release_impl = mir_connection_release_override;
}

mtf::UsingStubClientPlatform::~UsingStubClientPlatform()
{
    mir_connect_impl = prev_mir_connect_impl;
    mir_connection_release_impl = prev_mir_connection_release_impl;
}
