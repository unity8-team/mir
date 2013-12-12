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

/// \example demo_shell.cpp A simple mir shell

#include "window_manager.h"
#include "fullscreen_placement_strategy.h"
#include "translucent_outputs.h"
#include "../server_configuration.h"

#include "mir/run_mir.h"
#include "mir/report_exception.h"
#include "mir/graphics/display.h"
#include "mir/input/composite_event_filter.h"

#include <iostream>

namespace me = mir::examples;
namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mf = mir::frontend;
namespace mi = mir::input;

namespace mir
{
namespace examples
{

struct DemoServerConfiguration : mir::examples::ServerConfiguration
{
    DemoServerConfiguration(int argc, char const* argv[],
                            std::initializer_list<std::shared_ptr<mi::EventFilter>> const& filter_list)
      : ServerConfiguration(argc, argv),
        filter_list(filter_list)
    {
        namespace po = boost::program_options;

        add_options()
            ("translucent-output", po::value<bool>(),
                "Make the shell output surfaces contain transparency information (limited to nested mode yet) [bool:default=false]")
            ("fullscreen-surfaces", po::value<bool>(),
                "Make all surfaces fullscreen [bool:default=false]");
    }

    std::shared_ptr<mg::OutputConfiguration> the_output_configuration() override
    {
        return output_configuration(
            [this]() -> std::shared_ptr<mg::OutputConfiguration>
            {
                if (the_options()->is_set("translucent-output"))
                    return std::make_shared<TranslucentOutputs>();
                else
                    return DefaultServerConfiguration::the_output_configuration();
            });
    }

    std::shared_ptr<msh::PlacementStrategy> the_shell_placement_strategy() override
    {
        return shell_placement_strategy(
            [this]() -> std::shared_ptr<msh::PlacementStrategy>
            {
                if (the_options()->is_set("fullscreen-surfaces"))
                    return std::make_shared<me::FullscreenPlacementStrategy>(the_shell_display_layout());
                else
                    return DefaultServerConfiguration::the_shell_placement_strategy();
            });
    }

    std::shared_ptr<mi::CompositeEventFilter> the_composite_event_filter() override
    {
        auto composite_filter = ServerConfiguration::the_composite_event_filter();
        for (auto const& filter : filter_list)
            composite_filter->append(filter);

        return composite_filter;
    }

    std::vector<std::shared_ptr<mi::EventFilter>> const filter_list;
};

}
}

int main(int argc, char const* argv[])
try
{
    auto wm = std::make_shared<me::WindowManager>();
    me::DemoServerConfiguration config(argc, argv, {wm});

    mir::run_mir(config, [&config, &wm](mir::DisplayServer&)
        {
            // We use this strange two stage initialization to avoid a circular dependency between the EventFilters
            // and the SessionStore
            wm->set_focus_controller(config.the_focus_controller());
            wm->set_display(config.the_display());
            wm->set_compositor(config.the_compositor());
        });
    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
