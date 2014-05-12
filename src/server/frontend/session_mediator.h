/*
 * Copyright © 2012-2014 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_FRONTEND_SESSION_MEDIATOR_H_
#define MIR_FRONTEND_SESSION_MEDIATOR_H_

#include "mir_protobuf.pb.h"
#include "mir/frontend/connection_context.h"
#include "mir/frontend/surface_id.h"
#include "mir_toolkit/common.h"

#include <functional>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

namespace mir
{
namespace graphics
{
class Buffer;
class Platform;
class Display;
class GraphicBufferAllocator;
}

/// Frontend interface. Mediates the interaction between client
/// processes and the core of the mir system.
namespace frontend
{
class ClientBufferTracker;
class Shell;
class Session;
class Surface;
class ResourceCache;
class SessionMediatorReport;
class EventSink;
class DisplayChanger;
class Screencast;

// SessionMediator relays requests from the client process into the server.
class SessionMediator : public mir::protobuf::DisplayServer
{
public:

    SessionMediator(
        pid_t client_pid,
        std::shared_ptr<Shell> const& shell,
        std::shared_ptr<graphics::Platform> const& graphics_platform,
        std::shared_ptr<frontend::DisplayChanger> const& display_changer,
        std::vector<MirPixelFormat> const& surface_pixel_formats,
        std::shared_ptr<SessionMediatorReport> const& report,
        std::shared_ptr<EventSink> const& event_sink,
        std::shared_ptr<ResourceCache> const& resource_cache,
        std::shared_ptr<Screencast> const& screencast,
        ConnectionContext const& connection_context);

    ~SessionMediator() noexcept;

    /* Platform independent requests */
    void connect(::google::protobuf::RpcController* controller,
                 const ::mir::protobuf::ConnectParameters* request,
                 ::mir::protobuf::Connection* response,
                 ::google::protobuf::Closure* done) override;

    void create_surface(google::protobuf::RpcController* controller,
                        const mir::protobuf::SurfaceParameters* request,
                        mir::protobuf::Surface* response,
                        google::protobuf::Closure* done) override;

    void next_buffer(
        google::protobuf::RpcController* controller,
        mir::protobuf::SurfaceId const* request,
        mir::protobuf::Buffer* response,
        google::protobuf::Closure* done) override;

    void release_surface(google::protobuf::RpcController* controller,
                         const mir::protobuf::SurfaceId*,
                         mir::protobuf::Void*,
                         google::protobuf::Closure* done) override;

    void disconnect(google::protobuf::RpcController* controller,
                    const mir::protobuf::Void* request,
                    mir::protobuf::Void* response,
                    google::protobuf::Closure* done) override;

    void configure_surface(google::protobuf::RpcController* controller,
                           const mir::protobuf::SurfaceSetting*,
                           mir::protobuf::SurfaceSetting*,
                           google::protobuf::Closure* done) override;

    void configure_display(::google::protobuf::RpcController* controller,
                       const ::mir::protobuf::DisplayConfiguration* request,
                       ::mir::protobuf::DisplayConfiguration* response,
                       ::google::protobuf::Closure* done) override;

    void create_screencast(google::protobuf::RpcController*,
                           const mir::protobuf::ScreencastParameters*,
                           mir::protobuf::Screencast*,
                           google::protobuf::Closure* done) override;

    void release_screencast(google::protobuf::RpcController*,
                            const mir::protobuf::ScreencastId*,
                            mir::protobuf::Void*,
                            google::protobuf::Closure* done) override;

    void screencast_buffer(google::protobuf::RpcController*,
                           const mir::protobuf::ScreencastId*,
                           mir::protobuf::Buffer*,
                           google::protobuf::Closure* done) override;

    /* Platform specific requests */
    void drm_auth_magic(google::protobuf::RpcController* controller,
                        const mir::protobuf::DRMMagic* request,
                        mir::protobuf::DRMAuthMagicStatus* response,
                        google::protobuf::Closure* done) override;

    void new_fds_for_trusted_clients(
        ::google::protobuf::RpcController* controller,
        ::mir::protobuf::SocketFDRequest const* parameters,
        ::mir::protobuf::SocketFD* response,
        ::google::protobuf::Closure* done) override;

private:
    void pack_protobuf_buffer(protobuf::Buffer& protobuf_buffer,
                              graphics::Buffer* graphics_buffer,
                              bool need_full_ipc);

    void advance_buffer(SurfaceId surf_id, Surface& surface, std::function<void(graphics::Buffer*, bool)> complete);
    pid_t client_pid;
    std::shared_ptr<Shell> const shell;
    std::shared_ptr<graphics::Platform> const graphics_platform;

    std::vector<MirPixelFormat> const surface_pixel_formats;

    std::shared_ptr<frontend::DisplayChanger> const display_changer;
    std::shared_ptr<SessionMediatorReport> const report;
    std::shared_ptr<EventSink> const event_sink;
    std::shared_ptr<ResourceCache> const resource_cache;
    std::shared_ptr<Screencast> const screencast;

    std::unordered_map<SurfaceId,graphics::Buffer*> client_buffer_resource;
    std::unordered_map<SurfaceId, std::shared_ptr<ClientBufferTracker>> client_buffer_tracker;

    std::mutex session_mutex;
    std::weak_ptr<Session> weak_session;

    ConnectionContext const connection_context;
};

}
}

#endif /* MIR_FRONTEND_SESSION_MEDIATOR_H_ */
