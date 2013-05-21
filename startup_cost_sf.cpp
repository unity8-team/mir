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
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sf_app.h"
#include "eglapp.h"
#include "timer.h"


int main(int argc, char **argv)
{
    struct timeval start, end;
    gettimeofday(&start, NULL);
    int width = 512, height = 512;

    EGLNativeWindowType egl_window;
	kvant_sf_init(&egl_window, 0, 0, width, height, 1.0f);

    if (!kvant_egl_init(EGL_DEFAULT_DISPLAY, egl_window))
    {
        printf("Can't initialize EGL\n");
        return 1;
    }

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    kvant_egl_swap_buffers();

    gettimeofday(&end, NULL);
    double seconds = time_delta(&end, &start); 
    printf("Startup time: %.3fms\n", seconds);

    kvant_egl_shutdown();
    kvant_sf_shutdown();
	return 0;
}
