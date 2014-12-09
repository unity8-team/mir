/*
 * Copyright © 2013-2014 Canonical Ltd.
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

#include "demo_compositor.h"
#include "window_manager.h"
#include "fullscreen_placement_strategy.h"
#include "example_input_event_filter.h"
#include "example_display_configuration_policy.h"

#include "mir/server.h"
#include "mir/options/default_configuration.h"
#include "mir/run_mir.h"
#include "mir/report_exception.h"
#include "mir/graphics/display.h"
#include "mir/input/composite_event_filter.h"
#include "mir/compositor/display_buffer_compositor_factory.h"
#include "mir/compositor/destination_alpha.h"
#include "mir/compositor/renderer_factory.h"
#include "mir/shell/host_lifecycle_event_listener.h"

#include <iostream>

namespace me = mir::examples;
namespace ms = mir::scene;
namespace mg = mir::graphics;
namespace mf = mir::frontend;
namespace mi = mir::input;
namespace mo = mir::options;
namespace mc = mir::compositor;
namespace msh = mir::shell;

namespace mir
{
namespace examples
{
class NestedLifecycleEventListener : public msh::HostLifecycleEventListener
{
public:
    virtual void lifecycle_event_occurred(MirLifecycleState state) override
    {
        printf("Lifecycle event occurred : state = %d\n", state);
    }
};

class DisplayBufferCompositorFactory : public mc::DisplayBufferCompositorFactory
{
public:
    DisplayBufferCompositorFactory(
        std::shared_ptr<mg::GLProgramFactory> const& gl_program_factory,
        std::shared_ptr<mc::CompositorReport> const& report) :
        gl_program_factory(gl_program_factory),
        report(report)
    {
    }

    std::unique_ptr<mc::DisplayBufferCompositor> create_compositor_for(
        mg::DisplayBuffer& display_buffer) override
    {
        return std::unique_ptr<mc::DisplayBufferCompositor>(
            new me::DemoCompositor{display_buffer, *gl_program_factory, report});
    }

private:
    std::shared_ptr<mg::GLProgramFactory> const gl_program_factory;
    std::shared_ptr<mc::CompositorReport> const report;
};
}
}

int main(int argc, char const* argv[])
try
{
    mir::Server server;

    auto const quit_filter = me::make_quit_filter_for(server);
    me::add_display_configuration_options_to(server);

    auto const wm = std::make_shared<me::WindowManager>();
    server.add_init_callback([&]
        {
            server.the_composite_event_filter()->append(wm);
        });

    server.override_the_host_lifecycle_event_listener([]
       {
           return std::make_shared<me::NestedLifecycleEventListener>();
       });

    server.add_configuration_option("fullscreen-surfaces", "Make all surfaces fullscreen", mir::OptionType::null);
    server.override_the_placement_strategy([&]()
        -> std::shared_ptr<ms::PlacementStrategy>
        {
            if (server.get_options()->is_set("fullscreen-surfaces"))
                return std::make_shared<me::FullscreenPlacementStrategy>(server.the_shell_display_layout());
            else
                return std::shared_ptr<ms::PlacementStrategy>{};
        });

    server.override_the_display_buffer_compositor_factory([&]
        {
            return std::make_shared<me::DisplayBufferCompositorFactory>(
                server.the_gl_program_factory(),
                server.the_compositor_report());
        });

    server.add_init_callback([&]
        {
            // We use this strange two stage initialization to avoid a circular dependency between the EventFilters
            // and the SessionStore
            wm->set_focus_controller(server.the_focus_controller());
            wm->set_display(server.the_display());
            wm->set_compositor(server.the_compositor());
            wm->set_input_scene(server.the_input_scene());
        });

    server.set_command_line(argc, argv);
    server.apply_settings();
    server.run();
    return server.exited_normally() ? EXIT_SUCCESS : EXIT_FAILURE;
}
catch (...)
{
    mir::report_exception();
    return EXIT_FAILURE;
}
