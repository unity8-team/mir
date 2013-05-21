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

#include "eglapp.h"
#include <stdio.h>
#include <sys/time.h>
#include <GLES2/gl2.h>

static inline double time_delta(struct timeval* e, struct timeval* s)
{
    double start = ((double) s->tv_sec) + ((double) s->tv_usec / 1000000);
    double end = ((double) e->tv_sec) + ((double) e->tv_usec / 1000000);
    return end-start; 
}

int main(void)
{
    struct timeval start, end;
    gettimeofday(&start, NULL);
  
    int width = 512, height = 512;
    if (!mir_eglapp_init(&width, &height))
    {
        printf("Can't initialize EGL\n");
        return 1;
    }

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    mir_eglapp_swap_buffers();

    gettimeofday(&end, NULL);
    double seconds = time_delta(&end, &start); 
    printf("Startup time: %.3fms\n", seconds);

    mir_eglapp_shutdown();
    return 0;
}
