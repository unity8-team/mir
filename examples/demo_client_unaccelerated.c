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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir_toolkit/mir_client_library.h"

#include <assert.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>

static char const *socket_file = "/tmp/mir_socket";
static MirConnection *connection = 0;
static MirSurface *surface = 0;

static void surface_create_callback(MirSurface *new_surface, void *context)
{
    (void)context;
    surface = new_surface;
}

static void surface_next_callback(MirSurface * new_surface, void *context)
{
    (void)new_surface;
    (void)context;
}

static void surface_release_callback(MirSurface *old_surface, void *context)
{
    (void)old_surface;
    (void)context;
    surface = 0;
}

static void render_pattern(MirGraphicsRegion *region, uint32_t pf)
{
    uint32_t *pixel = (uint32_t*) region->vaddr;

    int i,j;
    for(i=0; i< region->width; i++)
    {
        for(j=0; j<region->height; j++)
        {
            pixel[j*region->width + i] = pf;
        }
    }
}

static MirPixelFormat find_8888_format(MirDisplayInfo *info)
{
    MirPixelFormat pf = mir_pixel_format_invalid;

    for (int i = 0; i < info->supported_pixel_format_items; ++i)
    {
        MirPixelFormat cur_pf = info->supported_pixel_format[i];
        if (cur_pf == mir_pixel_format_abgr_8888 ||
            cur_pf == mir_pixel_format_xbgr_8888 ||
            cur_pf == mir_pixel_format_argb_8888 ||
            cur_pf == mir_pixel_format_xrgb_8888)
        {
            pf = cur_pf;
            break;
        }
    }

    assert(pf != mir_pixel_format_invalid);
    return pf;
}

static void fill_pattern(uint32_t pattern[2], MirPixelFormat pf)
{
    switch(pf)
    {
    case mir_pixel_format_abgr_8888:
    case mir_pixel_format_xbgr_8888:
        pattern[0] = 0xFF00FF00;
        pattern[1] = 0xFFFF0000;
        break;

    case mir_pixel_format_argb_8888:
    case mir_pixel_format_xrgb_8888:
        pattern[0] = 0xFF00FF00;
        pattern[1] = 0xFF0000FF;
        break;

    default:
        assert(0 && "Invalid pixel format");
    };
}

int main(int argc, char* argv[])
{
    int arg;
    opterr = 0;
    while ((arg = getopt (argc, argv, "hf:")) != -1)
    {
        switch (arg)
        {
        case 'f':
            socket_file = optarg;
            break;

        case '?':
        case 'h':
        default:
            puts(argv[0]);
            puts("Usage:");
            puts("    -f <socket filename>");
            puts("    -h: this help text");
            return -1;
        }
    }

    puts("Starting");

    mir_wait_for(mir_connect(socket_file, __PRETTY_FUNCTION__, &connection));
    puts("Connected");

    assert(connection != NULL);
    assert(mir_connection_is_valid(connection));
    assert(strcmp(mir_connection_get_error_message(connection), "") == 0);

    MirDisplayInfo display_info;
    mir_connection_get_display_info(connection, &display_info);
    assert(display_info.supported_pixel_format_items > 0);

    MirPixelFormat pixel_format = find_8888_format(&display_info);

    MirSurfaceParameters const request_params =
        {__PRETTY_FUNCTION__, 640, 480, pixel_format, mir_buffer_usage_software};
    mir_wait_for(mir_surface_create(connection, &request_params, surface_create_callback, 0));
    puts("Surface created");

    assert(surface != NULL);
    assert(mir_surface_is_valid(surface));
    assert(strcmp(mir_surface_get_error_message(surface), "") == 0);

    uint32_t pattern[2] = {0};
    fill_pattern(pattern, pixel_format);

    MirGraphicsRegion graphics_region;
    int i=0; 
    while (1)
    {
        mir_wait_for(mir_surface_next_buffer(surface, surface_next_callback, 0 ));
        mir_surface_get_graphics_region( surface, &graphics_region);
        if ((i++ % 2) == 0)
            render_pattern(&graphics_region, pattern[0]);
        else 
            render_pattern(&graphics_region, pattern[1]);
    }

    mir_wait_for(mir_surface_release(surface, surface_release_callback, 0));
    puts("Surface released");

    mir_connection_release(connection);
    puts("Connection released");

    return 0;
}
