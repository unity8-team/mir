/*
 * Copyright © 2012-2014 Canonical Ltd.
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

#include "graphics.h"
#include "as_render_target.h"

#include "mir/server.h"
#include "mir/report_exception.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"
#include "mir/renderer/gl/render_target.h"

#include <csignal>

namespace mg=mir::graphics;
namespace mo=mir::options;
namespace me=mir::examples;

namespace
{
volatile std::sig_atomic_t running = true;

void signal_handler(int /*signum*/)
{
    running = false;
}
}

void render_loop(mir::Server& server)
{
    /* Set up graceful exit on SIGINT and SIGTERM */
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    auto display = server.the_display();

    mir::draw::glAnimationBasic gl_animation;

    display->for_each_display_sync_group([&](mg::DisplaySyncGroup& group)
    {
        group.for_each_display_buffer([&](mg::DisplayBuffer& buffer)
        {
            me::as_render_target(buffer)->make_current();
            gl_animation.init_gl();
        });
    });

    while (running)
    {
        display->for_each_display_sync_group([&](mg::DisplaySyncGroup& group)
        {
            group.for_each_display_buffer([&](mg::DisplayBuffer& buffer)
            {
                auto const render_target = me::as_render_target(buffer);
                render_target->make_current();
                gl_animation.render_gl();
                render_target->swap_buffers();
            });
            group.post();
        });

        gl_animation.step();
    }
}

int main(int argc, char const** argv)
try
{
    // We don't want to act as a server by providing an endpoint
    setenv("MIR_SERVER_NO_FILE", "", 1);

    // If there's a server available, try connecting to it
    if (auto const socket = getenv("MIR_SOCKET"))
        setenv("MIR_SERVER_HOST_SOCKET", socket, 0);

    mir::Server server;
    server.set_command_line(argc, argv);
    server.apply_settings();

    render_loop(server);

    return EXIT_SUCCESS;
}
catch (...)
{
    mir::report_exception();
    return EXIT_FAILURE;
}
