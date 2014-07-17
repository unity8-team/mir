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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

///\page basic.c basic.c: A simple mir client
/// demo_client shows the use of mir API.
/// This program opens a mir connection and creates a surface.
///\section demo_client demo_client()
/// Opens a mir connection and creates a surface and advances the
/// current buffer before closing the surface and connection.
///\subsection connect request and wait for connection handle
/// \snippet basic.c connect_tag
///\subsection surface_create request and wait for surface handle
/// \snippet basic.c surface_create_tag
///\subsection swap_buffers exchange the current buffer for a new one
/// \snippet basic.c swap_buffers_tag
///\subsection surface_release We release our surface
/// \snippet basic.c surface_release_tag
///\subsection connection_release We release our connection
/// \snippet basic.c connection_release_tag
///\subsection get the raw, platform-specific buffer handle for the current buffer
/// \snippet basic.c get_current_buffer_tag
/// \example basic.c A simple mir client
///\section MirDemoState MirDemoState
/// The handles needs to be accessible both to callbacks and to the control function.
/// \snippet basic.c MirDemoState_tag
///\section Callbacks Callbacks
/// This program opens a mir connection and creates a surface. The handles
/// needs to be accessible both to callbacks and to the control function.
/// \snippet basic.c Callback_tag

///\internal [MirDemoState_tag]
// Utility structure for the state of a single surface session.
typedef struct MirDemoState
{
    MirConnection *connection;
    MirSurface *surface;
} MirDemoState;
///\internal [MirDemoState_tag]

///\internal [Callback_tag]
// Callback to update MirDemoState on connection
static void connection_callback(MirConnection *new_connection, void *context)
{
    ((MirDemoState*)context)->connection = new_connection;
}

// Callback to update MirDemoState on surface_create
static void surface_create_callback(MirSurface *new_surface, void *context)
{
    ((MirDemoState*)context)->surface = new_surface;
}

// Callback to update MirDemoState on swap_buffers
static void surface_swap_buffers_callback(MirSurface* surface, void *context)
{
    (void) surface;
    (void) context;
}

// Callback to update MirDemoState on surface_release
static void surface_release_callback(MirSurface *old_surface, void *context)
{
    (void)old_surface;
    ((MirDemoState*)context)->surface = 0;
}
///\internal [Callback_tag]

MirDemoState mcd;

void demo_closedown()
{
    mir_connection_release(mcd.connection);
}

void demo_client(const char* server, int buffer_swap_count)
{  
    atexit(&demo_closedown);

    mcd.connection = 0;
    mcd.surface = 0;

    puts("Starting");

    ///\internal [connect_tag]
    // Call mir_connect and wait for callback to complete.
    mir_wait_for(mir_connect(server, __PRETTY_FUNCTION__, connection_callback, &mcd));
    puts("Connected");
    ///\internal [connect_tag]

    // We expect a connection handle;
    // we expect it to be valid; and,
    // we don't expect an error description
    assert(mcd.connection != NULL);
    assert(mir_connection_is_valid(mcd.connection));
    assert(strcmp(mir_connection_get_error_message(mcd.connection), "") == 0);

    // We can query information about the platform we're running on
    {
        MirPlatformPackage platform_package;
        platform_package.data_items = -1;
        platform_package.fd_items = -1;

        mir_connection_get_platform(mcd.connection, &platform_package);
        assert(0 <= platform_package.data_items);
        assert(0 <= platform_package.fd_items);
    }

    // Identify a supported pixel format
    MirPixelFormat pixel_format;
    unsigned int valid_formats;
    mir_connection_get_available_surface_formats(mcd.connection, &pixel_format, 1, &valid_formats);
    MirSurfaceParameters const request_params =
        {__PRETTY_FUNCTION__, 640, 480, pixel_format,
         mir_buffer_usage_hardware, mir_display_output_id_invalid};

    ///\internal [surface_create_tag]
    // ...we create a surface using that format and wait for callback to complete.
    mir_wait_for(mir_connection_create_surface(mcd.connection, &request_params, surface_create_callback, &mcd));
    puts("Surface created");
    ///\internal [surface_create_tag]

    // We expect a surface handle;
    // we expect it to be valid; and,
    // we don't expect an error description
    assert(mcd.surface != NULL);
    assert(mir_surface_is_valid(mcd.surface));
    assert(strcmp(mir_surface_get_error_message(mcd.surface), "") == 0);

    // We can query the surface parameters...
    {
        MirSurfaceParameters response_params;
        mir_surface_get_parameters(mcd.surface, &response_params);

        // ...and they should match the request
        assert(request_params.width == response_params.width);
        assert(request_params.height ==  response_params.height);
        assert(request_params.pixel_format == response_params.pixel_format);
    }

    // We can keep exchanging the current buffer for a new one
    for (int i = 0; i < buffer_swap_count; i++)
    {
        // We can query the current graphics buffer attributes
        {
            ///\internal [get_current_buffer_tag]
            MirNativeBuffer* buffer_package = NULL;
            mir_surface_get_current_buffer(mcd.surface, &buffer_package);
            assert(buffer_package != NULL);
            if (mir_platform_type_gbm == mir_surface_get_platform_type(mcd.surface))
            {
                // Interpret buffer_package as MirBufferPackage
            } else if (mir_platform_type_android == mir_surface_get_platform_type(mcd.surface))
            {
                // Interpret buffer_package as ANativeWindowBuffer
            }
            ///\internal [get_current_buffer_tag]
            // In a real application we'd render into the current buffer
        }

        ///\internal [swap_buffers_tag]
        mir_wait_for(mir_surface_swap_buffers(mcd.surface, surface_swap_buffers_callback, &mcd));
        ///\internal [swap_buffers_tag]
    }

    ///\internal [surface_release_tag]
    // We should release our surface
    mir_wait_for(mir_surface_release(mcd.surface, surface_release_callback, &mcd));
    puts("Surface released");
    ///\internal [surface_release_tag]

    ///\internal [connection_release_tag]
    // We should release our connection
//    mir_connection_release(mcd.connection);
    puts("Connection released");
    ///\internal [connection_release_tag]
}

// The main() function deals with parsing arguments and defaults
int main(int argc, char* argv[])
{
    // Some variables for holding command line options
    char const *server = NULL;
    int buffer_swap_count = 0;

    // Parse the command line
    {
        int arg;
        opterr = 0;
        while ((arg = getopt (argc, argv, "c:hm:")) != -1)
        {
            switch (arg)
            {
            case 'c':
                buffer_swap_count = atoi(optarg);
                break;
            case 'm':
                server = optarg;
                break;

            case '?':
            case 'h':
            default:
                puts(argv[0]);
                puts("Usage:");
                puts("    -m <Mir server socket>");
                puts("    -h: this help text");
                return -1;
            }
        }
    }

    demo_client(server, buffer_swap_count);
    return 0;
}
