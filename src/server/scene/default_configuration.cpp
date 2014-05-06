/*
 * Copyright © 2013-2014 Canonical Ltd.
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

#include "mir/default_server_configuration.h"

#include "mir/graphics/display.h"
#include "mir/graphics/gl_context.h"
#include "mir/input/input_configuration.h"
#include "mir/input/input_dispatcher_configuration.h"
#include "mir/abnormal_exit.h"
#include "mir/scene/session.h"

#include "broadcasting_session_event_sink.h"
#include "default_session_container.h"
#include "gl_pixel_buffer.h"
#include "global_event_sender.h"
#include "mediating_display_changer.h"
#include "session_container.h"
#include "session_manager.h"
#include "surface_allocator.h"
#include "surface_controller.h"
#include "surface_stack.h"
#include "threaded_snapshot_strategy.h"

namespace mc = mir::compositor;
namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;

std::shared_ptr<ms::SurfaceStackModel>
mir::DefaultServerConfiguration::the_surface_stack_model()
{
    return surface_stack(
        [this]()
        {
            auto const scene_report = the_scene_report();

            auto const ss = std::make_shared<ms::SurfaceStack>(
                the_input_registrar(),
                scene_report);

            the_input_configuration()->the_input_dispatcher_configuration()->set_input_targets(ss);

            return ss;
        });
}

std::shared_ptr<mc::Scene>
mir::DefaultServerConfiguration::the_scene()
{
    return surface_stack(
        [this]()
        {
            auto const scene_report = the_scene_report();

            auto const ss = std::make_shared<ms::SurfaceStack>(
                the_input_registrar(),
                scene_report);

            the_input_configuration()->the_input_dispatcher_configuration()->set_input_targets(ss);

            return ss;
        });
}

auto mir::DefaultServerConfiguration::the_surface_factory()
-> std::shared_ptr<ms::SurfaceFactory>
{
    return surface_factory(
        [this]()
        {
            return std::make_shared<ms::SurfaceAllocator>(
                the_buffer_stream_factory(),
                the_input_channel_factory(),
                the_surface_configurator(),
                the_scene_report());
        });
}

std::shared_ptr<ms::SurfaceCoordinator>
mir::DefaultServerConfiguration::the_surface_coordinator()
{
    return surface_coordinator(
        [this]()
        {
            return std::make_shared<ms::SurfaceController>(
                the_surface_factory(),
                the_placement_strategy(),
                the_surface_stack_model());
        });
}

std::shared_ptr<ms::BroadcastingSessionEventSink>
mir::DefaultServerConfiguration::the_broadcasting_session_event_sink()
{
    return broadcasting_session_event_sink(
        []
        {
            return std::make_shared<ms::BroadcastingSessionEventSink>();
        });
}

std::shared_ptr<ms::SessionEventSink>
mir::DefaultServerConfiguration::the_session_event_sink()
{
    return the_broadcasting_session_event_sink();
}

std::shared_ptr<ms::SessionEventHandlerRegister>
mir::DefaultServerConfiguration::the_session_event_handler_register()
{
    return the_broadcasting_session_event_sink();
}

std::shared_ptr<ms::SessionContainer>
mir::DefaultServerConfiguration::the_session_container()
{
    return session_container(
        []{ return std::make_shared<ms::DefaultSessionContainer>(); });
}

std::shared_ptr<ms::MediatingDisplayChanger>
mir::DefaultServerConfiguration::the_mediating_display_changer()
{
    return mediating_display_changer(
        [this]()
        {
            return std::make_shared<ms::MediatingDisplayChanger>(
                the_display(),
                the_compositor(),
                the_display_configuration_policy(),
                the_session_container(),
                the_session_event_handler_register());
        });

}

std::shared_ptr<mf::DisplayChanger>
mir::DefaultServerConfiguration::the_frontend_display_changer()
{
    return the_mediating_display_changer();
}

std::shared_ptr<mir::DisplayChanger>
mir::DefaultServerConfiguration::the_display_changer()
{
    return the_mediating_display_changer();
}

std::shared_ptr<mf::EventSink>
mir::DefaultServerConfiguration::the_global_event_sink()
{
    return global_event_sink(
        [this]()
        {
            return std::make_shared<ms::GlobalEventSender>(the_session_container());
        });
}

std::shared_ptr<ms::SessionManager>
mir::DefaultServerConfiguration::the_session_manager()
{
    return session_manager(
        [this]() -> std::shared_ptr<ms::SessionManager>
        {
            return std::make_shared<ms::SessionManager>(
                the_surface_coordinator(),
                the_session_container(),
                the_shell_focus_setter(),
                the_snapshot_strategy(),
                the_session_event_sink(),
                the_session_listener());
        });
}

std::shared_ptr<mf::Shell>
mir::DefaultServerConfiguration::the_frontend_shell()
{
    return the_session_manager();
}

std::shared_ptr<msh::FocusController>
mir::DefaultServerConfiguration::the_focus_controller()
{
    return the_session_manager();
}

std::shared_ptr<ms::PixelBuffer>
mir::DefaultServerConfiguration::the_pixel_buffer()
{
    return pixel_buffer(
        [this]()
        {
            return std::make_shared<ms::GLPixelBuffer>(
                the_display()->create_gl_context());
        });
}

std::shared_ptr<ms::SnapshotStrategy>
mir::DefaultServerConfiguration::the_snapshot_strategy()
{
    return snapshot_strategy(
        [this]()
        {
            return std::make_shared<ms::ThreadedSnapshotStrategy>(
                the_pixel_buffer());
        });
}
