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
class MemoryRegion;
}
}

class MirBufferStream : public mir::client::ClientSurface
{
public:
    virtual ~MirBufferStream() = default;

    virtual MirWaitHandle* next_buffer(
        mir_buffer_stream_callback callback, void* context) = 0;

    virtual EGLNativeWindowType egl_native_window() = 0;

    virtual void get_cpu_region(MirGraphicsRegion& region) = 0;

    /* mir::client::ClientSurface */
    MirSurfaceParameters get_parameters() const = 0;
    std::shared_ptr<mir::client::ClientBuffer> get_current_buffer() = 0;
    void request_and_wait_for_next_buffer() = 0;
    void request_and_wait_for_configure(MirSurfaceAttrib a, int value) = 0;
    
    virtual mir::protobuf::BufferStreamId protobuf_id() const = 0;
    virtual MirNativeBuffer* get_current_buffer_package() = 0;
    virtual MirPlatformType platform_type() = 0;

protected:
    MirBufferStream() = default;
    MirBufferStream(MirBufferStream const&) = delete;
    MirBufferStream& operator=(MirBufferStream const&) = delete;
};

#endif /* MIR_CLIENT_MIR_BUFFER_STREAM_H_ */
