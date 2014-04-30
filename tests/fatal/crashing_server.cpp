/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 *
 *
 * This is a test server designed to crash on composition of the first frame.
 * It's designed to test the quality of the core files and stack traces
 * that result.
 */

#include "mir/server_configuration.h"
#include "mir/default_server_configuration.h"
#include "mir/compositor/display_buffer_compositor_factory.h"
#include "mir/compositor/display_buffer_compositor.h"
#include "mir/report_exception.h"
#include "mir/run_mir.h"
#include <iostream>

using namespace mir;
using namespace mir::compositor;

class CrashingServerConfiguration : public DefaultServerConfiguration
{
public:
    CrashingServerConfiguration(int argc, char const* argv[])
        : DefaultServerConfiguration(argc, argv)
    {
    }

    std::shared_ptr<DisplayBufferCompositorFactory>
        the_display_buffer_compositor_factory() override
    {
        class CrashingDisplayBufferCompositor : public DisplayBufferCompositor
        {
        public:
            bool composite() override
            {
                throw std::runtime_error("He's dead, Jim");
                return false;
            }
        };

        class CrashingDisplayBufferCompositorFactory :
            public DisplayBufferCompositorFactory
        {
        public:
            std::unique_ptr<DisplayBufferCompositor>
                create_compositor_for(graphics::DisplayBuffer&) override
            {
                auto raw = new CrashingDisplayBufferCompositor();
                return std::unique_ptr<CrashingDisplayBufferCompositor>(raw);
            }
        };

        return std::make_shared<CrashingDisplayBufferCompositorFactory>();
    }
};

int main(int argc, char const* argv[])
try
{
    CrashingServerConfiguration config(argc, argv);

    run_mir(config, [](mir::DisplayServer&){} );
    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
