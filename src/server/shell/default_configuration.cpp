/*
 * Copyright © 2013 Canonical Ltd.
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

#include "broadcasting_session_event_sink.h"
#include "consuming_placement_strategy.h"
#include "default_focus_mechanism.h"
#include "gl_pixel_buffer.h"
#include "global_event_sender.h"
#include "graphics_display_layout.h"
#include "mediating_display_changer.h"
#include "organising_surface_factory.h"
#include "threaded_snapshot_strategy.h"

#include "session_container.h"
#include "default_shell.h"
#include "mir/shell/session.h"

#include "mir/graphics/display.h"
#include "mir/graphics/gl_context.h"

namespace ms = mir::surfaces;
namespace msh = mir::shell;
namespace mf = mir::frontend;

std::shared_ptr<msh::DefaultShell>
mir::DefaultServerConfiguration::the_default_shell()
{
    return default_shell(
        [this]() -> std::shared_ptr<msh::DefaultShell>
        {
            return std::make_shared<msh::DefaultShell>(
                the_shell_surface_factory(),
                the_shell_focus_setter(),
                the_shell_snapshot_strategy(),
                the_shell_session_event_sink(),
                the_shell_session_listener());
        });
}

std::shared_ptr<mf::Shell>
mir::DefaultServerConfiguration::the_frontend_shell()
{
    return the_default_shell();
}

std::shared_ptr<msh::FocusController>
mir::DefaultServerConfiguration::the_focus_controller()
{
    return the_default_shell();
}

std::shared_ptr<msh::SurfaceFactory>
mir::DefaultServerConfiguration::the_shell_surface_factory()
{
    return shell_surface_factory(
        [this]()
        {
            return std::make_shared<msh::OrganisingSurfaceFactory>(
                the_surfaces_surface_factory(),
                the_shell_placement_strategy());
        });
}

std::shared_ptr<msh::BroadcastingSessionEventSink>
mir::DefaultServerConfiguration::the_broadcasting_session_event_sink()
{
    return broadcasting_session_event_sink(
        []
        {
            return std::make_shared<msh::BroadcastingSessionEventSink>();
        });
}

std::shared_ptr<msh::SessionEventSink>
mir::DefaultServerConfiguration::the_shell_session_event_sink()
{
    return the_broadcasting_session_event_sink();
}

std::shared_ptr<msh::SessionEventHandlerRegister>
mir::DefaultServerConfiguration::the_shell_session_event_handler_register()
{
    return the_broadcasting_session_event_sink();
}

std::shared_ptr<msh::PlacementStrategy>
mir::DefaultServerConfiguration::the_shell_placement_strategy()
{
    return shell_placement_strategy(
        [this]
        {
            return std::make_shared<msh::ConsumingPlacementStrategy>(
                the_shell_display_layout());
        });
}

std::shared_ptr<msh::FocusSetter>
mir::DefaultServerConfiguration::the_shell_focus_setter()
{
    return shell_focus_setter(
        [this]
        {
            return std::make_shared<msh::DefaultFocusMechanism>(
                the_input_targeter(),
                the_shell_surface_controller());
        });
}

std::shared_ptr<msh::SessionContainer>
mir::DefaultServerConfiguration::the_shell_session_container()
{
    return the_default_shell();
}

std::shared_ptr<msh::PixelBuffer>
mir::DefaultServerConfiguration::the_shell_pixel_buffer()
{
    return shell_pixel_buffer(
        [this]()
        {
            return std::make_shared<msh::GLPixelBuffer>(
                the_display()->create_gl_context());
        });
}

std::shared_ptr<msh::DisplayLayout>
mir::DefaultServerConfiguration::the_shell_display_layout()
{
    return shell_display_layout(
        [this]()
        {
            return std::make_shared<msh::GraphicsDisplayLayout>(the_display());
        });
}

std::shared_ptr<msh::MediatingDisplayChanger>
mir::DefaultServerConfiguration::the_mediating_display_changer()
{
    return mediating_display_changer(
        [this]()
        {
            return std::make_shared<msh::MediatingDisplayChanger>(
                the_display(),
                the_compositor(),
                the_display_configuration_policy(),
                the_shell_session_container(),
                the_shell_session_event_handler_register());
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

std::shared_ptr<msh::SnapshotStrategy>
mir::DefaultServerConfiguration::the_shell_snapshot_strategy()
{
    return shell_snapshot_strategy(
        [this]()
        {
            return std::make_shared<msh::ThreadedSnapshotStrategy>(
                the_shell_pixel_buffer());
        });
}

std::function<void()>
mir::DefaultServerConfiguration::force_threads_to_unblock_callback()
{
    auto shell_sessions = the_shell_session_container();
    return [shell_sessions]
    {
        shell_sessions->for_each([](std::shared_ptr<msh::Session> const& session)
        {
            session->force_requests_to_complete();
        });
    };
}

std::shared_ptr<mf::EventSink>
mir::DefaultServerConfiguration::the_global_event_sink()
{
    return global_event_sink(
        [this]()
        {
            return std::make_shared<msh::GlobalEventSender>(the_shell_session_container());
        });
}
