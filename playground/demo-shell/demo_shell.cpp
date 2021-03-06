/*
 * Copyright © 2013-2015 Canonical Ltd.
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
#include "../server_configuration.h"

#include "mir/run_mir.h"
#include "mir/report_exception.h"
#include "mir/graphics/display.h"
#include "mir/input/composite_event_filter.h"
#include "mir/compositor/display_buffer_compositor_factory.h"
#include "mir/renderer/renderer_factory.h"
#include "mir/options/option.h"
#include "server_example_host_lifecycle_event_listener.h"

#include <iostream>

namespace me = mir::examples;
namespace ms = mir::scene;
namespace mg = mir::graphics;
namespace mf = mir::frontend;
namespace mi = mir::input;
namespace mc = mir::compositor;
namespace msh = mir::shell;

namespace mir
{
namespace examples
{
class DisplayBufferCompositorFactory : public mc::DisplayBufferCompositorFactory
{
public:
    DisplayBufferCompositorFactory(
        std::shared_ptr<mc::CompositorReport> const& report) :
        report(report)
    {
    }

    std::unique_ptr<mc::DisplayBufferCompositor> create_compositor_for(
        mg::DisplayBuffer& display_buffer) override
    {
        return std::unique_ptr<mc::DisplayBufferCompositor>(
            new me::DemoCompositor{display_buffer, report});
    }

private:
    std::shared_ptr<mc::CompositorReport> const report;
};

class DemoServerConfiguration : public mir::examples::ServerConfiguration
{
public:
    using mir::examples::ServerConfiguration::ServerConfiguration;

    std::shared_ptr<compositor::DisplayBufferCompositorFactory> the_display_buffer_compositor_factory() override
    {
        return display_buffer_compositor_factory(
            [this]()
            {
                return std::make_shared<me::DisplayBufferCompositorFactory>(
                    the_compositor_report());
            });
    }

    std::shared_ptr<msh::HostLifecycleEventListener> the_host_lifecycle_event_listener() override
    {
       return host_lifecycle_event_listener(
           [this]()
           {
               return std::make_shared<HostLifecycleEventListener>(the_logger());
           });
    }
};

}
}

int main(int argc, char const* argv[])
try
{
    me::DemoServerConfiguration config(argc, argv);

    auto wm = std::make_shared<me::WindowManager>();

    mir::run_mir(config, [&config, &wm](mir::DisplayServer&)
        {
            // We use this strange two stage initialization to avoid a circular dependency between the EventFilters
            // and the SessionStore
            wm->set_focus_controller(config.the_focus_controller());
            wm->set_display(config.the_display());
            wm->set_compositor(config.the_compositor());
            wm->set_input_scene(config.the_input_scene());

            config.the_composite_event_filter()->prepend(wm);
        });
    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
