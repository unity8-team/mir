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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */
#ifndef MIR_CLIENT_MIR_SURFACE_H_
#define MIR_CLIENT_MIR_SURFACE_H_

#include "mir_protobuf.pb.h"

#include "mir/geometry/pixel_format.h"
#include "mir/geometry/dimensions.h"
#include "mir_toolkit/mir_client_library.h"
#include "mir_toolkit/common.h"
#include "client_buffer_depository.h"
#include "mir_wait_handle.h"
#include "mir_client_surface.h"
#include "client_platform.h"

#include <memory>

namespace mir
{
namespace client
{
class ClientBuffer;
struct MemoryRegion;
}
}

struct mir_toolkit::MirSurface : public mir::client::ClientSurface
{
public:
    MirSurface(MirSurface const &) = delete;
    MirSurface& operator=(MirSurface const &) = delete;

    MirSurface(
        MirConnection *allocating_connection,
        mir::protobuf::DisplayServer::Stub & server,
        const std::shared_ptr<mir::client::Logger>& logger,
        const std::shared_ptr<mir::client::ClientBufferDepository>& depository,
        MirSurfaceParameters const & params,
        mir_surface_lifecycle_callback callback, void * context);

    ~MirSurface();

    MirWaitHandle* release_surface(
        mir_surface_lifecycle_callback callback,
        void *context);

    MirSurfaceParameters get_parameters() const;
    char const * get_error_message();
    int id() const;
    bool is_valid() const;
    MirWaitHandle* next_buffer(mir_surface_lifecycle_callback callback, void * context);
    MirWaitHandle* get_create_wait_handle();

    std::shared_ptr<MirBufferPackage> get_current_buffer_package();
    std::shared_ptr<mir::client::ClientBuffer> get_current_buffer();
    void get_cpu_region(MirGraphicsRegion& region);
    void release_cpu_region();
    EGLNativeWindowType generate_native_window();

    MirWaitHandle* configure(MirSurfaceAttrib a, int value);
    int attrib(MirSurfaceAttrib a) const;

private:
    void on_configured();
    void process_incoming_buffer();
    void populate(MirBufferPackage& buffer_package);
    void created(mir_surface_lifecycle_callback callback, void * context);
    void new_buffer(mir_surface_lifecycle_callback callback, void * context);
    mir::geometry::PixelFormat convert_ipc_pf_to_geometry(google::protobuf::int32 pf);

    /* todo: race condition. protobuf does not guarantee that callbacks will be synchronized. potential
             race in surface, last_buffer_id */
    mir::protobuf::DisplayServer::Stub & server;
    mir::protobuf::Surface surface;
    std::string error_message;

    MirConnection *connection;
    MirWaitHandle create_wait_handle;
    MirWaitHandle next_buffer_wait_handle;
    MirWaitHandle configure_wait_handle;

    std::shared_ptr<mir::client::MemoryRegion> secured_region;
    std::shared_ptr<mir::client::ClientBufferDepository> buffer_depository;

    std::shared_ptr<mir::client::Logger> logger;
    std::shared_ptr<EGLNativeWindowType> accelerated_window;

    mir::protobuf::SurfaceSetting configure_result;

    // Cache of latest SurfaceSettings returned from the server
    int attrib_cache[mir_surface_attrib_arraysize_];
};

#endif /* MIR_CLIENT_PRIVATE_MIR_WAIT_HANDLE_H_ */
