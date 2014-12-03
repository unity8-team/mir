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

#include "mir_toolkit/mir_buffer_stream.h"
#include "mir_buffer_stream.h"
#include "mir_connection.h"

#include <stdexcept>
#include <boost/throw_exception.hpp>

namespace
{
void null_callback(MirBufferStream*, void*) {}
}

// TODO: Add async variant?

MirBufferStream* mir_connection_create_buffer_stream_sync(
    MirConnection* connection,
    int width, int height, MirPixelFormat pixel_format, MirBufferUsage buffer_usage)
{
    if (!MirConnection::is_valid(connection))
        return nullptr;

    MirBufferStream* buffer_stream = nullptr;

    try
    {
        auto const client_platform = connection->get_client_platform();
        mir::geometry::Size const size{width, height};

        std::unique_ptr<MirBufferStream> buffer_stream_uptr{
            new MirBufferStream{connection,
                size,
                pixel_format, buffer_usage,
                connection->display_server(),
                client_platform,
                client_platform->create_buffer_factory(),
                null_callback, nullptr}};

        buffer_stream_uptr->creation_wait_handle()->wait_for_all();

        if (buffer_stream_uptr->valid())
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

void mir_buffer_stream_release_sync(MirBufferStream* buffer_stream)
{
    buffer_stream->release(null_callback, nullptr)->wait_for_all();
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
// assign_result is compatible with all 2-parameter callbacks
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

// TODO: Decl
MirEGLNativeWindowType mir_buffer_stream_egl_native_window(MirBufferStream* buffer_stream)
{
    return reinterpret_cast<MirEGLNativeWindowType>(buffer_stream->egl_native_window());
}
