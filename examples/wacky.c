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
 * Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "eglapp.h"
#include <stdio.h>
#include <unistd.h>
#include <GLES2/gl2.h>

int main(int argc, char *argv[])
{
    unsigned int width = 0, height = 0;

    printf("=== Wacky colour frame scheduling demo ===\n"
           "You should see alternating red/blue colours.\n"
           "If you see green then you've got a compositor bug.\n"
           "\n");

    if (!mir_eglapp_init(argc, argv, &width, &height))
        return 1;

    float const opacity = mir_eglapp_background_opacity;

    while (mir_eglapp_running())
    {
        glClearColor(opacity, 0.0f, 0.0f, opacity);
        glClear(GL_COLOR_BUFFER_BIT);
        mir_eglapp_swap_buffers();
        sleep(1);

        glClearColor(0.0f, opacity, 0.0f, opacity);
        glClear(GL_COLOR_BUFFER_BIT);
        mir_eglapp_swap_buffers();

        /*
         * We race the compositor here.
         * It may show our green screen for one frame, but even if it does
         * it should be replaced with the blue screen (below) immediately,
         * and stay blue for a full second, as that is most definitely
         * the newest frame.
         */

        glClearColor(0.0f, 0.0f, opacity, opacity);
        glClear(GL_COLOR_BUFFER_BIT);
        mir_eglapp_swap_buffers();
        sleep(1);
    }

    mir_eglapp_shutdown();

    return 0;
}
