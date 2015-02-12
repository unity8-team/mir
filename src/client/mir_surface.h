/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */
#ifndef MIR_CLIENT_MIR_SURFACE_H_
#define MIR_CLIENT_MIR_SURFACE_H_

#include "mir_protobuf.pb.h"

#include "mir/geometry/dimensions.h"
#include "mir_toolkit/mir_client_library.h"
#include "mir_toolkit/common.h"
#include "mir/graphics/native_buffer.h"
#include "mir/optional_value.h"
#include "client_buffer_depository.h"
#include "mir_wait_handle.h"
#include "mir/client_platform.h"
#include "client_buffer_stream.h"

#include <memory>
#include <functional>
#include <mutex>
#include <unordered_set>

namespace mir
{
namespace dispatch
{
class Dispatchable;
}
namespace input
{
namespace receiver
{
class InputPlatform;
}
}
namespace client
{
class ClientBuffer;
class ClientBufferStream;
class ClientBufferStreamFactory;

struct MemoryRegion;
}
}

struct MirSurfaceSpec
{
    MirSurfaceSpec() = default;
    MirSurfaceSpec(MirConnection* connection, int width, int height, MirPixelFormat format);
    MirSurfaceSpec(MirConnection* connection, MirSurfaceParameters const& params);

    mir::protobuf::SurfaceParameters serialize() const;

    // Required parameters
    MirConnection* connection{nullptr};
    int width{-1};
    int height{-1};
    MirPixelFormat pixel_format{mir_pixel_format_invalid};
    MirBufferUsage buffer_usage{mir_buffer_usage_hardware};

    // Optional parameters
    mir::optional_value<std::string> surface_name;
    mir::optional_value<uint32_t> output_id;

    mir::optional_value<MirSurfaceType> type;
    mir::optional_value<MirSurfaceState> state;
    mir::optional_value<MirOrientationMode> pref_orientation;

    mir::optional_value<MirSurface*> parent;
    mir::optional_value<MirRectangle> aux_rect;
    mir::optional_value<MirEdgeAttachment> edge_attachment;
};

struct MirSurface
{
public:
    MirSurface(MirSurface const &) = delete;
    MirSurface& operator=(MirSurface const &) = delete;

    MirSurface(std::string const& error);

    MirSurface(
        MirConnection *allocating_connection,
        mir::protobuf::DisplayServer::Stub& server,
        mir::protobuf::Debug::Stub* debug,
        std::shared_ptr<mir::client::ClientBufferStreamFactory> const& buffer_stream_factory,
        std::shared_ptr<mir::input::receiver::InputPlatform> const& input_platform,
        MirSurfaceSpec const& spec,
        mir_surface_callback callback, void * context);

    ~MirSurface();

    MirWaitHandle* release_surface(
        mir_surface_callback callback,
        void *context);

    MirSurfaceParameters get_parameters() const;
    char const * get_error_message();
    int id() const;

    MirWaitHandle* get_create_wait_handle();
    MirWaitHandle* configure(MirSurfaceAttrib a, int value);

    // TODO: Some sort of extension mechanism so that this can be moved
    //       out into a separate class in the libmirclient-debug DSO.
    bool translate_to_screen_coordinates(int x, int y,
                                         int* screen_x, int* screen_y);
    
    // Non-blocking
    int attrib(MirSurfaceAttrib a) const;

    MirOrientation get_orientation() const;
    MirWaitHandle* set_preferred_orientation(MirOrientationMode mode);

    MirWaitHandle* configure_cursor(MirCursorConfiguration const* cursor);

    void set_event_handler(MirEventDelegate const* delegate);
    void handle_event(MirEvent const& e);

    void request_and_wait_for_configure(MirSurfaceAttrib a, int value);

    mir::client::ClientBufferStream* get_buffer_stream();

    static bool is_valid(MirSurface* query);
private:
    mutable std::mutex mutex; // Protects all members of *this

    void on_configured();
    void on_cursor_configured();
    void created(mir_surface_callback callback, void* context);
    MirPixelFormat convert_ipc_pf_to_geometry(google::protobuf::int32 pf) const;

    mir::protobuf::DisplayServer::Stub* server{nullptr};
    mir::protobuf::Debug::Stub* debug{nullptr};
    mir::protobuf::Surface surface;
    mir::protobuf::BufferRequest buffer_request;
    std::string error_message;
    std::string name;
    mir::protobuf::Void void_response;

    MirConnection* const connection{nullptr};

    MirWaitHandle create_wait_handle;
    MirWaitHandle configure_wait_handle;
    MirWaitHandle configure_cursor_wait_handle;

    std::shared_ptr<mir::client::ClientBufferStreamFactory> const buffer_stream_factory;
    std::shared_ptr<mir::client::ClientBufferStream> buffer_stream;
    std::shared_ptr<mir::input::receiver::InputPlatform> const input_platform;

    mir::protobuf::SurfaceSetting configure_result;

    // Cache of latest SurfaceSettings returned from the server
    int attrib_cache[mir_surface_attribs];
    MirOrientation orientation = mir_orientation_normal;

    std::function<void(MirEvent const*)> handle_event_callback;
    std::shared_ptr<mir::dispatch::Dispatchable> input_dispatcher;
};

#endif /* MIR_CLIENT_PRIVATE_MIR_WAIT_HANDLE_H_ */
