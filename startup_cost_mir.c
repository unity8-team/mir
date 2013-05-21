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
 * Author: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "timer.h"
#include "mir_app.h"
#include "eglapp.h"
#include <stdio.h>
#include <GLES2/gl2.h>

int main(void)
{
    struct timeval start, end;
    gettimeofday(&start, NULL);
    int width = 512, height = 512;
  
    EGLNativeDisplayType egl_display;
    EGLNativeWindowType egl_window;
    kvant_mir_connect(&egl_display, &egl_window, width, height); 

    if (!kvant_egl_init(egl_display, egl_window))
    {
        printf("Can't initialize EGL\n");
        return 1;
    }

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    kvant_egl_swap_buffers();

    gettimeofday(&end, NULL);
    double seconds = time_delta(&end, &start); 
    printf("Startup time: %.3fs\n", seconds);

    kvant_egl_shutdown();
    kvant_mir_shutdown();
    return 0;
}
