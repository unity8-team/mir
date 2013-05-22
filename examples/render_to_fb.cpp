/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/platform.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"

#include "mir/logging/display_report.h"
#include "mir/logging/dumb_console_logger.h"

#include "graphics.h"

#include <csignal>

namespace mg=mir::graphics;
namespace ml=mir::logging;

namespace
{

volatile std::sig_atomic_t running = true;

void signal_handler(int /*signum*/)
{
    running = false;
}

}

int main(int, char**)
{
    /* Set up graceful exit on SIGINT and SIGTERM */
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    auto logger = std::make_shared<ml::DumbConsoleLogger>();
    auto platform = mg::create_platform(std::make_shared<ml::DisplayReport>(logger));
    auto display = platform->create_display();

    mir::draw::glAnimationBasic gl_animation;

    display->for_each_display_buffer([&](mg::DisplayBuffer& buffer)
    {
        buffer.make_current();
        gl_animation.init_gl();
    });

    while (running)
    {
        display->for_each_display_buffer([&](mg::DisplayBuffer& buffer)
        {
            buffer.make_current();

            gl_animation.render_gl();

            buffer.post_update();
        });

        gl_animation.step();
    }

    return 0;
}
