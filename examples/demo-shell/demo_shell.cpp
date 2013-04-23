/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "application_switcher.h"
#include "fullscreen_placement_strategy.h"
#include "software_cursor_overlay_renderer.h"

#include "mir/run_mir.h"
#include "mir/default_server_configuration.h"
#include "mir/shell/session_manager.h"
#include "mir/shell/registration_order_focus_sequence.h"
#include "mir/shell/single_visibility_focus_mechanism.h"
#include "mir/shell/session_container.h"
#include "mir/shell/organising_surface_factory.h"
#include "mir/graphics/display.h"

#include <boost/exception/diagnostic_information.hpp>
#include <iostream>

namespace me = mir::examples;
namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mf = mir::frontend;
namespace mi = mir::input;
namespace mc = mir::compositor;

namespace mir
{
namespace examples
{

struct DemoServerConfiguration : mir::DefaultServerConfiguration
{
    DemoServerConfiguration(int argc, char const* argv[],
                            std::initializer_list<std::shared_ptr<mi::EventFilter> const> const& filter_list)
      : DefaultServerConfiguration(argc, argv),
        filter_list(filter_list),
        software_cursor_renderer(std::make_shared<me::SoftwareCursorOverlayRenderer>())
    {
    }

    std::shared_ptr<msh::PlacementStrategy> the_shell_placement_strategy() override
    {
        return shell_placement_strategy(
            [this]
            {
                return std::make_shared<me::FullscreenPlacementStrategy>(the_display());
            });
    }

    std::initializer_list<std::shared_ptr<mi::EventFilter> const> the_event_filters() override
    {
        return filter_list;
    }
    
    std::shared_ptr<mc::OverlayRenderer> the_overlay_renderer() override
    {
        return software_cursor_renderer;
    }

    std::shared_ptr<mi::CursorListener> the_cursor_listener() override
    {
        return software_cursor_renderer;
    }

    std::initializer_list<std::shared_ptr<mi::EventFilter> const> const filter_list;
    std::shared_ptr<me::SoftwareCursorOverlayRenderer> software_cursor_renderer;
};

}
}

int main(int argc, char const* argv[])
try
{
    auto app_switcher = std::make_shared<me::ApplicationSwitcher>();
    me::DemoServerConfiguration config(argc, argv, {app_switcher});
    
    mir::run_mir(config, [&config, &app_switcher](mir::DisplayServer&)
        {
            // We use this strange two stage initialization to avoid a circular dependency between the EventFilters
            // and the SessionStore
            app_switcher->set_focus_controller(config.the_focus_controller());
            config.software_cursor_renderer->set_damage_handler(config.the_compositor());
        });
    return 0;
}
catch (std::exception const& error)
{
    std::cerr << "ERROR: " << boost::diagnostic_information(error) << std::endl;
    return 1;
} 
