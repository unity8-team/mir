/*
 * Copyright © 2012 Canonical Ltd.
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

#include "mir/frontend/session_mediator.h"
#include "mir/frontend/session_mediator_report.h"
#include "mir/frontend/shell.h"
#include "mir/frontend/session.h"
#include "mir/frontend/surface.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/frontend/display_changer.h"
#include "mir/frontend/resource_cache.h"
#include "mir_toolkit/common.h"
#include "mir/graphics/buffer_id.h"
#include "mir/graphics/buffer.h"
#include "mir/surfaces/buffer_stream.h"
#include "mir/graphics/graphic_buffer_allocator.h"
#include "mir/geometry/dimensions.h"
#include "mir/graphics/platform.h"
#include "mir/frontend/display_changer.h"
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir/frontend/client_constants.h"
#include "mir/geometry/rectangles.h"
#include "client_buffer_tracker.h"
#include "protobuf_buffer_packer.h"

#include <boost/throw_exception.hpp>

namespace msh = mir::shell;
namespace mf = mir::frontend;
namespace mfd = mir::frontend::detail;
namespace geom = mir::geometry;
namespace mg = mir::graphics;

mf::SessionMediator::SessionMediator(
    std::shared_ptr<frontend::Shell> const& shell,
    std::shared_ptr<graphics::Platform> const & graphics_platform,
    std::shared_ptr<mf::DisplayChanger> const& display_changer,
    std::shared_ptr<graphics::GraphicBufferAllocator> const& buffer_allocator,
    std::shared_ptr<SessionMediatorReport> const& report,
    std::shared_ptr<EventSink> const& sender,
    std::shared_ptr<ResourceCache> const& resource_cache) :
    shell(shell),
    graphics_platform(graphics_platform),
    buffer_allocator(buffer_allocator),
    display_changer(display_changer),
    report(report),
    event_sink(sender),
    resource_cache(resource_cache),
    client_tracker(std::make_shared<ClientBufferTracker>(frontend::client_buffer_cache_size))
{
}

mf::SessionMediator::~SessionMediator() noexcept
{
    if (session)
    {
        report->session_error(session->name(), __PRETTY_FUNCTION__, "connection dropped without disconnect");
        shell->close_session(session);
    }
}

void mf::SessionMediator::connect(
    ::google::protobuf::RpcController*,
    const ::mir::protobuf::ConnectParameters* request,
    ::mir::protobuf::Connection* response,
    ::google::protobuf::Closure* done)
{
    report->session_connect_called(request->application_name());

    {
        std::unique_lock<std::mutex> lock(session_mutex);
        session = shell->open_session(request->application_name(), event_sink);
    }

    auto ipc_package = graphics_platform->get_ipc_package();
    auto platform = response->mutable_platform();

    for (auto& data : ipc_package->ipc_data)
        platform->add_data(data);

    for (auto& ipc_fds : ipc_package->ipc_fds)
        platform->add_fd(ipc_fds);

    auto display_config = display_changer->active_configuration();
    auto protobuf_config = response->mutable_display_configuration();
    mfd::pack_protobuf_display_configuration(*protobuf_config, *display_config);

    resource_cache->save_resource(response, ipc_package);

    done->Run();
}

void mf::SessionMediator::create_surface(
    google::protobuf::RpcController* /*controller*/,
    const mir::protobuf::SurfaceParameters* request,
    mir::protobuf::Surface* response,
    google::protobuf::Closure* done)
{
    {
        std::unique_lock<std::mutex> lock(session_mutex);

        if (session.get() == nullptr)
            BOOST_THROW_EXCEPTION(std::logic_error("Invalid application session"));

        report->session_create_surface_called(session->name());

        auto const id = shell->create_surface_for(session,
            msh::SurfaceCreationParameters()
            .of_name(request->surface_name())
            .of_size(request->width(), request->height())
            .of_buffer_usage(static_cast<graphics::BufferUsage>(request->buffer_usage()))
            .of_pixel_format(static_cast<geometry::PixelFormat>(request->pixel_format()))
            .with_output_id(graphics::DisplayConfigurationOutputId(request->output_id()))
            );

        {
            auto surface = session->get_surface(id);
            response->mutable_id()->set_value(id.as_value());
            response->set_width(surface->size().width.as_uint32_t());
            response->set_height(surface->size().height.as_uint32_t());
            response->set_pixel_format((int)surface->pixel_format());
            response->set_buffer_usage(request->buffer_usage());

            if (surface->supports_input())
                response->add_fd(surface->client_input_fd());

            client_buffer_resource = surface->advance_client_buffer();
            auto const& id = client_buffer_resource->id();

            auto buffer = response->mutable_buffer();
            buffer->set_buffer_id(id.as_uint32_t());

            if (!client_tracker->client_has(id))
            {
                auto packer = std::make_shared<mfd::ProtobufBufferPacker>(buffer);
                graphics_platform->fill_ipc_package(packer, client_buffer_resource);
            }
            client_tracker->add(id);
        }
    }

    done->Run();
}

void mf::SessionMediator::next_buffer(
    ::google::protobuf::RpcController* /*controller*/,
    ::mir::protobuf::SurfaceId const* request,
    ::mir::protobuf::Buffer* response,
    ::google::protobuf::Closure* done)
{
    {
        std::unique_lock<std::mutex> lock(session_mutex);

        if (session.get() == nullptr)
            BOOST_THROW_EXCEPTION(std::logic_error("Invalid application session"));

        report->session_next_buffer_called(session->name());

        auto surface = session->get_surface(SurfaceId(request->value()));

        client_buffer_resource.reset();
        client_buffer_resource = surface->advance_client_buffer();
    }

    auto const& id = client_buffer_resource->id();
    response->set_buffer_id(id.as_uint32_t());

    if (!client_tracker->client_has(id))
    {
        auto packer = std::make_shared<mfd::ProtobufBufferPacker>(response);
        graphics_platform->fill_ipc_package(packer, client_buffer_resource);
    }
    client_tracker->add(id);
    done->Run();
}

void mf::SessionMediator::release_surface(
    google::protobuf::RpcController* /*controller*/,
    const mir::protobuf::SurfaceId* request,
    mir::protobuf::Void*,
    google::protobuf::Closure* done)
{
    {
        std::unique_lock<std::mutex> lock(session_mutex);

        if (session.get() == nullptr)
            BOOST_THROW_EXCEPTION(std::logic_error("Invalid application session"));

        report->session_release_surface_called(session->name());

        auto const id = SurfaceId(request->value());

        session->destroy_surface(id);
    }

    done->Run();
}

void mf::SessionMediator::disconnect(
    google::protobuf::RpcController* /*controller*/,
    const mir::protobuf::Void* /*request*/,
    mir::protobuf::Void* /*response*/,
    google::protobuf::Closure* done)
{
    {
        std::unique_lock<std::mutex> lock(session_mutex);

        if (session.get() == nullptr)
            BOOST_THROW_EXCEPTION(std::logic_error("Invalid application session"));

        report->session_disconnect_called(session->name());

        shell->close_session(session);
        session.reset();
    }

    done->Run();
}

void mf::SessionMediator::configure_surface(
    google::protobuf::RpcController*, // controller,
    const mir::protobuf::SurfaceSetting* request,
    mir::protobuf::SurfaceSetting* response,
    google::protobuf::Closure* done)
{
    MirSurfaceAttrib attrib = static_cast<MirSurfaceAttrib>(request->attrib());

    // Required response fields:
    response->mutable_surfaceid()->CopyFrom(request->surfaceid());
    response->set_attrib(attrib);

    {
        std::unique_lock<std::mutex> lock(session_mutex);

        if (session.get() == nullptr)
            BOOST_THROW_EXCEPTION(std::logic_error("Invalid application session"));

        report->session_configure_surface_called(session->name());

        auto const id = frontend::SurfaceId(request->surfaceid().value());
        int value = request->ivalue();
        int newvalue = session->configure_surface(id, attrib, value);

        response->set_ivalue(newvalue);
    }

    done->Run();
}

void mf::SessionMediator::configure_display(
    ::google::protobuf::RpcController*,
    const ::mir::protobuf::DisplayConfiguration* request,
    ::mir::protobuf::DisplayConfiguration* response,
    ::google::protobuf::Closure* done)
{
    {
        std::unique_lock<std::mutex> lock(session_mutex);
        auto config = display_changer->active_configuration();
        for (auto i=0; i < request->display_output_size(); i++)
        {   
            auto& output = request->display_output(i);
            mg::DisplayConfigurationOutputId output_id{static_cast<int>(output.output_id())};
            config->configure_output(output_id, output.used(),
                                     geom::Point{output.position_x(), output.position_y()},
                                     output.current_mode());
        }

        display_changer->configure(session, config);
        auto display_config = display_changer->active_configuration();
        mfd::pack_protobuf_display_configuration(*response, *display_config);
    }
    done->Run();
}
