/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "surfaceless_buffer_stream.h"

#include "mir_buffer_stream.h"
#include "mir_connection.h"

#include <stdexcept>
#include <boost/throw_exception.hpp>

namespace mcl = mir::client;

namespace
{
void null_callback(MirBufferStream*, void*) {}
}

// TODO: Add async variant?

MirBufferStream* mir_connection_create_surfaceless_buffer_stream_sync(
    MirConnection* connection,
    int width, int height, MirPixelFormat pixel_format, MirBufferUsage buffer_usage)
{
    if (!MirConnection::is_valid(connection))
        return nullptr;

    mcl::SurfacelessBufferStream* buffer_stream = nullptr;

    try
    {
        auto const client_platform = connection->get_client_platform();
        mir::geometry::Size const size{width, height};

        std::unique_ptr<mcl::SurfacelessBufferStream> buffer_stream_uptr{
            new mcl::SurfacelessBufferStream{connection,
                size,
                pixel_format, buffer_usage,
                connection->display_server(),
                client_platform,
                client_platform->create_buffer_factory(),
                null_callback, nullptr}};

        buffer_stream_uptr->creation_wait_handle()->wait_for_all();

        if (buffer_stream_uptr->is_valid())
        {
            buffer_stream = buffer_stream_uptr.get();
            buffer_stream_uptr.release();
        }
    }
    catch (std::exception const&)
    {
        return nullptr;
    }

    return buffer_stream;
}

void mir_surfaceless_buffer_stream_release_sync(MirBufferStream* buffer_stream)
{
    auto stream = dynamic_cast<mcl::SurfacelessBufferStream*>(buffer_stream);
    stream->release(null_callback, nullptr)->wait_for_all();
    delete buffer_stream;
}

void mir_buffer_stream_get_current_buffer(MirBufferStream* buffer_stream, MirNativeBuffer** buffer_package_out)
{
    *buffer_package_out = buffer_stream->get_current_buffer_package();
}

MirWaitHandle* mir_buffer_stream_swap_buffers(
    MirBufferStream* buffer_stream,
    mir_buffer_stream_callback callback,
    void* context)
try
{
    return buffer_stream->next_buffer(callback, context);
}
catch (std::exception const&)
{
    return nullptr;
}

namespace
{
void assign_result(void* result, void** context)
{
    if (context)
        *context = result;
}
}

void mir_buffer_stream_swap_buffers_sync(MirBufferStream* buffer_stream)
{
    mir_wait_for(mir_buffer_stream_swap_buffers(buffer_stream,
        reinterpret_cast<mir_buffer_stream_callback>(assign_result),
        nullptr));
}

void mir_buffer_stream_get_graphics_region(
    MirBufferStream *buffer_stream,
    MirGraphicsRegion *graphics_region)
{
    return buffer_stream->get_cpu_region(*graphics_region);
}

MirEGLNativeWindowType mir_buffer_stream_get_egl_native_window(MirBufferStream* buffer_stream)
{
    return reinterpret_cast<MirEGLNativeWindowType>(buffer_stream->egl_native_window());
}

MirPlatformType mir_buffer_stream_get_platform_type(MirBufferStream* buffer_stream)
{
    return buffer_stream->platform_type();
}
