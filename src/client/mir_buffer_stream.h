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

#ifndef MIR_CLIENT_MIR_BUFFER_STREAM_H_
#define MIR_CLIENT_MIR_BUFFER_STREAM_H_

#include "mir_client_surface.h"
#include "mir_wait_handle.h"
#include "client_buffer_depository.h"
#include "mir_toolkit/client_types.h"
#include "mir_toolkit/mir_native_buffer.h"
#include "mir_protobuf.pb.h"
#include "mir/geometry/size.h"
#include "mir/geometry/rectangle.h"

#include <EGL/eglplatform.h>

namespace mir
{
namespace protobuf { class DisplayServer; }
namespace client
{
class ClientBufferFactory;
class EGLNativeWindowFactory;
}
}

struct MirBufferStream : public mir::client::ClientSurface
{
public:
    MirBufferStream(
        MirConnection *connection,
        mir::geometry::Size const& size,
        MirPixelFormat pixel_format,
        MirBufferUsage buffer_usage,
        mir::protobuf::DisplayServer& server,
        std::shared_ptr<mir::client::EGLNativeWindowFactory> const& egl_native_window_factory,
        std::shared_ptr<mir::client::ClientBufferFactory> const& factory,
        mir_buffer_stream_callback callback, void* context);

    MirWaitHandle* creation_wait_handle();
    bool valid();

    MirWaitHandle* release(
        mir_buffer_stream_callback callback, void* context);

    MirWaitHandle* next_buffer(
        mir_buffer_stream_callback callback, void* context);

    EGLNativeWindowType egl_native_window();

    /* mir::client::ClientSurface */
    MirSurfaceParameters get_parameters() const;
    std::shared_ptr<mir::client::ClientBuffer> get_current_buffer();
    void request_and_wait_for_next_buffer();
    void request_and_wait_for_configure(MirSurfaceAttrib a, int value);
    
    MirNativeBuffer* get_current_buffer_package();
    
    mir::protobuf::BufferStreamId protobuf_id() const;

private:
    void process_buffer(mir::protobuf::Buffer const& buffer);
    void buffer_stream_created(
        mir_buffer_stream_callback callback, void* context);
    void released(
        mir_buffer_stream_callback callback, void* context);
    void next_buffer_received(
        mir_buffer_stream_callback callback, void* context);
    
    MirConnection *connection;
    
    mir::geometry::Size size;
    MirPixelFormat pixel_format;
    MirBufferUsage buffer_usage;

    mir::protobuf::DisplayServer& server;
    std::shared_ptr<mir::client::EGLNativeWindowFactory> const egl_native_window_factory;
    mir::client::ClientBufferDepository buffer_depository;
    std::shared_ptr<EGLNativeWindowType> egl_native_window_;

    mir::protobuf::BufferStream protobuf_buffer_stream;
    mir::protobuf::Buffer protobuf_buffer;
    mir::protobuf::Void protobuf_void;

    MirWaitHandle create_buffer_stream_wait_handle;
    MirWaitHandle release_wait_handle;
    MirWaitHandle next_buffer_wait_handle;
};

#endif /* MIR_CLIENT_MIR_BUFFER_STREAM_H_ */
