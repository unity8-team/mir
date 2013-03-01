/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Guest <thomas.guest@canonical.com>
 */

#include "mir_toolkit/mir_client_library.h"
#include "mir_toolkit/mir_client_library_drm.h"
#include "mir_toolkit/mir_client_library_lightdm.h"

#include "mir_connection.h"
#include "mir_surface.h"
#include "native_client_platform_factory.h"
#include "mir_logger.h"
#include "make_rpc_channel.h"

#include "mir_protobuf.pb.h"

#include <set>
#include <unordered_set>
#include <cstddef>

namespace mcl = mir::client;
namespace mp = mir::protobuf;
namespace gp = google::protobuf;

std::mutex mir_toolkit::MirConnection::connection_guard;
std::unordered_set<mir_toolkit::MirConnection*> mir_toolkit::MirConnection::valid_connections;

namespace
{
mir_toolkit::MirConnection error_connection;
}

mir_toolkit::MirWaitHandle* mir_toolkit::mir_connect(char const* socket_file, char const* name, MirConnection **result)
{

    try
    {
        auto log = std::make_shared<mcl::ConsoleLogger>();
        auto client_platform_factory = std::make_shared<mcl::NativeClientPlatformFactory>();

        MirConnection* connection = new MirConnection(
            mcl::make_rpc_channel(socket_file, log),
            log,
            client_platform_factory);

        return connection->connect(name, result);
    }
    catch (std::exception const& x)
    {
        error_connection.set_error_message(x.what());

        // Test cases expect a non-null error connection but why not null
        // for simpler error handling?
        if (result)
            *result = &error_connection;

        return 0;
    }
}

int mir_toolkit::mir_connection_is_valid(MirConnection * connection)
{
    return MirConnection::is_valid(connection);
}

char const * mir_toolkit::mir_connection_get_error_message(MirConnection * connection)
{
    return connection->get_error_message();
}

void mir_toolkit::mir_connection_release(MirConnection * connection)
{
    if (&error_connection == connection) return;

    auto wait_handle = connection->disconnect();
    wait_handle->wait_for_result();

    delete connection;
}

mir_toolkit::MirEGLNativeDisplayType mir_toolkit::mir_connection_get_egl_native_display(MirConnection *connection)
{
    return connection->egl_native_display();
}

mir_toolkit::MirWaitHandle* mir_toolkit::mir_surface_create(
    MirConnection * connection,
    MirSurfaceParameters const * params,
    mir_surface_lifecycle_callback callback,
    void * context)
{
    if (&error_connection == connection) return 0;

    try
    {
        return connection->create_surface(*params, callback, context);
    }
    catch (std::exception const&)
    {
        // TODO callback with an error surface
        return 0; // TODO
    }

}

mir_toolkit::MirWaitHandle* mir_toolkit::mir_surface_release(
    MirSurface * surface,
    mir_surface_lifecycle_callback callback, void * context)
{
    return surface->release_surface(callback, context);
}

int mir_toolkit::mir_debug_surface_id(MirSurface * surface)
{
    return surface->id();
}

int mir_toolkit::mir_surface_is_valid(MirSurface* surface)
{
    return surface->is_valid();
}

char const * mir_toolkit::mir_surface_get_error_message(MirSurface * surface)
{
    return surface->get_error_message();
}

void mir_toolkit::mir_surface_get_parameters(MirSurface * surface, MirSurfaceParameters *parameters)
{
    *parameters = surface->get_parameters();
}

void mir_toolkit::mir_surface_get_current_buffer(MirSurface *surface, MirBufferPackage * buffer_package_out)
{
    auto package = surface->get_current_buffer_package();

    buffer_package_out->data_items = package->data_items;
    buffer_package_out->fd_items = package->fd_items;
    for(auto i=0; i<mir_buffer_package_max; i++)
    {
        buffer_package_out->data[i] = package->data[i];
        buffer_package_out->fd[i] = package->fd[i];
    }

    buffer_package_out->stride = package->stride;
}

void mir_toolkit::mir_connection_get_platform(MirConnection *connection, MirPlatformPackage *platform_package)
{
    connection->populate(*platform_package);
}

void mir_toolkit::mir_connection_get_display_info(MirConnection *connection, MirDisplayInfo *display_info)
{
    connection->populate(*display_info);
}

void mir_toolkit::mir_surface_get_graphics_region(MirSurface * surface, MirGraphicsRegion * graphics_region)
{
    surface->get_cpu_region( *graphics_region);
}

mir_toolkit::MirWaitHandle* mir_toolkit::mir_surface_next_buffer(MirSurface *surface, mir_surface_lifecycle_callback callback, void * context)
{
    return surface->next_buffer(callback, context);
}

void mir_toolkit::mir_wait_for(MirWaitHandle* wait_handle)
{
    if (wait_handle)
        wait_handle->wait_for_result();
}

void mir_toolkit::mir_callback_on(MirWaitHandle *wait,
                     mir_generic_callback cb,
                     void *context)
{
    if (wait)
        wait->register_callback(cb, context);
}

mir_toolkit::MirEGLNativeWindowType mir_toolkit::mir_surface_get_egl_native_window(MirSurface *surface)
{
    return surface->generate_native_window();
}

mir_toolkit::MirWaitHandle *mir_toolkit::mir_connection_drm_auth_magic(MirConnection* connection,
                                             unsigned int magic,
                                             mir_drm_auth_magic_callback callback,
                                             void* context)
{
    return connection->drm_auth_magic(magic, callback, context);
}

mir_toolkit::MirWaitHandle *mir_toolkit::mir_connect_with_lightdm_id(
    char const *server,
    int lightdm_id,
    char const *app_name,
    MirConnection **result)
try
{
    auto log = std::make_shared<mcl::ConsoleLogger>();
    auto client_platform_factory = std::make_shared<mcl::NativeClientPlatformFactory>();

    MirConnection* connection = new MirConnection(
        mcl::make_rpc_channel(server, log),
        log,
        client_platform_factory);

    return connection->connect(lightdm_id, app_name, result);
}
catch (std::exception const& x)
{
    error_connection.set_error_message(x.what());
    if (result)
        *result = 0;
    return 0;
}

void mir_toolkit::mir_select_focus_by_lightdm_id(MirConnection* connection, int lightdm_id)
try
{
    connection->select_focus_by_lightdm_id(lightdm_id);
}
catch (std::exception const&)
{
    // Ignore
}
