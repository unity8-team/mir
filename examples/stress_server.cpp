/*
 * Copyright © 2012, 2013 Canonical Ltd.
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
 */

#include "basic_server_configuration.h"

#include "mir/compositor/compositor.h"
#include "mir/report_exception.h"
#include "mir/run_mir.h"
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>

#include <iostream>
#include <thread>

namespace
{

void set_power(MirPowerMode mode, mir::examples::BasicServerConfiguration& config, int counter)
{
    std::shared_ptr<mir::graphics::Display> display = config.the_display();
    std::shared_ptr<mir::graphics::DisplayConfiguration> displayConfig = display->configuration();
    std::shared_ptr<mir::compositor::Compositor> compositor = config.the_compositor();

    std::cout << "stopping compositor: " << counter << std::endl;
    compositor->stop();

    //displayConfig->for_each_output(
    //   [&](const mir::graphics::UserDisplayConfigurationOutput displayConfigOutput) {
    //       displayConfigOutput.power_mode = mode;
    //   }
    //);

    bool const power_on = mode == MirPowerMode::mir_power_mode_on;

    if (power_on)
    {
        std::cout << "starting compositor: " << counter << std::endl;
        compositor->start();
    }

}
void stress_compositor_thread(bool& done, mir::examples::BasicServerConfiguration& config)
{
    int counter = 0;
    MirPowerMode power_mode = MirPowerMode::mir_power_mode_off;
    while (!done)
    {
        counter++;
        power_mode = (power_mode == MirPowerMode::mir_power_mode_off) ?
                MirPowerMode::mir_power_mode_on : MirPowerMode::mir_power_mode_off;

        set_power(power_mode, config, counter);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
}

int main(int argc, char const* argv[])
try
{
    mir::examples::BasicServerConfiguration config(argc, argv);

    struct ScopeGuard
    {
        ~ScopeGuard()
        {
            if (a_thread.joinable())
            {
                done = true;
                a_thread.join();
            }
        }
        bool done;
        std::thread a_thread;
    } guard;

    run_mir(config, [&](mir::DisplayServer&){
        config.launch_client();
        guard.done = false;
        guard.a_thread = std::thread(stress_compositor_thread, std::ref(guard.done), std::ref(config));
    });
    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}

