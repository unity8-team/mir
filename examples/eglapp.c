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
 * Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "eglapp.h"
#include "mir_toolkit/mir_client_library.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <xkbcommon/xkbcommon-keysyms.h>

float mir_eglapp_background_opacity = 1.0f;

static const char appname[] = "egldemo";

static MirConnection *connection;
static MirSurface *surface;
static EGLDisplay egldisplay;
static EGLSurface eglsurface;
static volatile sig_atomic_t running = 0;

#define CHECK(_cond, _err) \
    if (!(_cond)) \
    { \
        printf("%s\n", (_err)); \
        return 0; \
    }

void mir_eglapp_shutdown(void)
{
    eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(egldisplay);
    mir_surface_release_sync(surface);
    surface = NULL;
    mir_connection_release(connection);
    connection = NULL;
}

static void shutdown(int signum)
{
    if (running)
    {
        running = 0;
        printf("Signal %d received. Good night.\n", signum);
    }
}

mir_eglapp_bool mir_eglapp_running(void)
{
    return running;
}

void mir_eglapp_swap_buffers(void)
{
    EGLint width, height;

    if (!running)
        return;

    eglSwapBuffers(egldisplay, eglsurface);

    /*
     * Querying the surface (actually the current buffer) dimensions here is
     * the only truly safe way to be sure that the dimensions we think we
     * have are those of the buffer being rendered to. But this should be
     * improved in future; https://bugs.launchpad.net/mir/+bug/1194384
     */
    if (eglQuerySurface(egldisplay, eglsurface, EGL_WIDTH, &width) &&
        eglQuerySurface(egldisplay, eglsurface, EGL_HEIGHT, &height))
    {
        glViewport(0, 0, width, height);
    }
}

static void mir_eglapp_handle_event(MirSurface* surface, MirEvent const* ev, void* context)
{
    (void) surface;
    (void) context;
    if (ev->type == mir_event_type_key &&
        ev->key.key_code == XKB_KEY_q &&
        ev->key.action == mir_key_action_up)
    {
        running = 0;
    }
    else if (ev->type == mir_event_type_resize)
    {
        /*
         * FIXME: https://bugs.launchpad.net/mir/+bug/1194384
         * It is unsafe to set the width and height here because we're in a
         * different thread to that doing the rendering. So we either need
         * support for event queuing (directing them to another thread) or
         * full single-threaded callbacks. (LP: #1194384).
         */
        printf("Resized to %dx%d\n", ev->resize.width, ev->resize.height);
    }
    else if (ev->type == mir_event_type_surface &&
             ev->surface.attrib == mir_surface_attrib_visibility)
    {
        if (ev->surface.value == mir_surface_visibility_exposed)
            printf("Surface exposed\n");
        else if (ev->surface.value == mir_surface_visibility_occluded)
            printf("Surface occluded\n");
    }
}

static const MirDisplayOutput *find_active_output(
    const MirDisplayConfiguration *conf)
{
    const MirDisplayOutput *output = NULL;
    int d;

    for (d = 0; d < (int)conf->num_outputs; d++)
    {
        const MirDisplayOutput *out = conf->outputs + d;

        if (out->used &&
            out->connected &&
            out->num_modes &&
            out->current_mode < out->num_modes)
        {
            output = out;
            break;
        }
    }

    return output;
}

mir_eglapp_bool mir_eglapp_init(int argc, char *argv[],
                                unsigned int *width, unsigned int *height)
{
    EGLint ctxattribs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    MirEventDelegate delegate = 
    {
        mir_eglapp_handle_event,
        NULL
    };
    EGLConfig eglconfig;
    EGLint neglconfigs;
    EGLContext eglctx;
    EGLBoolean ok;
    EGLint swapinterval = 1;
    unsigned int output_id = mir_display_output_id_invalid;
    char *mir_socket = NULL;
    char const* cursor_name = mir_default_cursor_name;

    if (argc > 1)
    {
        int i;
        for (i = 1; i < argc; i++)
        {
            mir_eglapp_bool help = 0;
            const char *arg = argv[i];

            if (arg[0] == '-')
            {
                switch (arg[1])
                {
                case 'b':
                    {
                        float alpha = 1.0f;
                        arg += 2;
                        if (!arg[0] && i < argc-1)
                        {
                            i++;
                            arg = argv[i];
                        }
                        if (sscanf(arg, "%f", &alpha) == 1)
                        {
                            mir_eglapp_background_opacity = alpha;
                        }
                        else
                        {
                            printf("Invalid opacity value: %s\n", arg);
                            help = 1;
                        }
                    }
                    break;
                case 'n':
                    swapinterval = 0;
                    break;
                case 'o':
                    {
                        unsigned int the_id = 0;
                        arg += 2;
                        if (!arg[0] && i < argc-1)
                        {
                            i++;
                            arg = argv[i];
                        }
                        if (sscanf(arg, "%u", &the_id) == 1)
                        {
                            output_id = the_id;
                        }
                        else
                        {
                            printf("Invalid output ID: %s\n", arg);
                            help = 1;
                        }
                    }
                    break;
                case 'f':
                    *width = 0;
                    *height = 0;
                    break;
                case 's':
                    {
                        unsigned int w, h;
                        arg += 2;
                        if (!arg[0] && i < argc-1)
                        {
                            i++;
                            arg = argv[i];
                        }
                        if (sscanf(arg, "%ux%u", &w, &h) == 2)
                        {
                            *width = w;
                            *height = h;
                        }
                        else
                        {
                            printf("Invalid size: %s\n", arg);
                            help = 1;
                        }
                    }
                    break;
                case 'm':
                    mir_socket = argv[++i];
                    break;
                case 'c':
                    cursor_name = argv[++i];
                    break;
                case 'q':
                    {
                        FILE *unused = freopen("/dev/null", "a", stdout);
                        (void)unused;
                        break;
                    }
                case 'h':
                default:
                    help = 1;
                    break;
                }
            }
            else
            {
                help = 1;
            }

            if (help)
            {
                printf("Usage: %s [<options>]\n"
                       "  -b               Background opacity (0.0 - 1.0)\n"
                       "  -h               Show this help text\n"
                       "  -f               Force full screen\n"
                       "  -o ID            Force placement on output monitor ID\n"
                       "  -n               Don't sync to vblank\n"
                       "  -m socket        Mir server socket\n"
                       "  -s WIDTHxHEIGHT  Force surface size\n"
                       "  -c name          Request cursor image by name\n"
                       "  -q               Quiet mode (no messages output)\n"
                       , argv[0]);
                return 0;
            }
        }
    }

    connection = mir_connect_sync(mir_socket, appname);
    CHECK(mir_connection_is_valid(connection), "Can't get connection");

    /* eglapps are interested in the screen size, so
       use mir_connection_create_display_config */
    MirDisplayConfiguration* display_config =
        mir_connection_create_display_config(connection);

    const MirDisplayOutput *output = find_active_output(display_config);

    if (output == NULL)
    {
        printf("No active outputs found.\n");
        return 0;
    }

    const MirDisplayMode *mode = &output->modes[output->current_mode];

    unsigned int format[mir_pixel_formats];
    unsigned int nformats;

    mir_connection_get_available_surface_formats(connection,
        format, mir_pixel_formats, &nformats);

    MirPixelFormat pixel_format = format[0];
    for (unsigned int f = 0; f < nformats; f++)
    {
        const int opaque = (format[f] == mir_pixel_format_xbgr_8888 ||
                            format[f] == mir_pixel_format_xrgb_8888 ||
                            format[f] == mir_pixel_format_bgr_888);

        if ((mir_eglapp_background_opacity == 1.0f && opaque) ||
            (mir_eglapp_background_opacity < 1.0f && !opaque))
        {
            pixel_format = format[f];
            break;
        }
    }

    printf("Current active output is %dx%d %+d%+d\n",
           mode->horizontal_resolution, mode->vertical_resolution,
           output->position_x, output->position_y);

    if (*width == 0)
        *width = mode->horizontal_resolution;
    if (*height == 0)
        *height = mode->vertical_resolution;

    mir_display_config_destroy(display_config);

    printf("Server supports %d of %d surface pixel formats. Using format: %d\n",
        nformats, mir_pixel_formats, pixel_format);
    unsigned int bpp = 8 * MIR_BYTES_PER_PIXEL(pixel_format);
    EGLint attribs[] =
    {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
        EGL_BUFFER_SIZE, bpp,
        EGL_NONE
    };

    MirSurfaceSpec *spec =
        mir_connection_create_spec_for_normal_surface(connection, *width, *height, pixel_format);

    CHECK(spec != NULL, "Can't create a surface spec");

    mir_surface_spec_set_name(spec, "eglappsurface");
    if (output_id != mir_display_output_id_invalid)
        mir_surface_spec_set_fullscreen_on_output(spec, output_id);

    surface = mir_surface_create_sync(spec);
    mir_surface_spec_release(spec);

    CHECK(mir_surface_is_valid(surface), "Can't create a surface");

    mir_surface_set_event_handler(surface, &delegate);
    
    MirCursorConfiguration *conf = mir_cursor_configuration_from_name(cursor_name);
    mir_surface_configure_cursor(surface, conf);
    mir_cursor_configuration_destroy(conf);

    egldisplay = eglGetDisplay(
                    mir_connection_get_egl_native_display(connection));
    CHECK(egldisplay != EGL_NO_DISPLAY, "Can't eglGetDisplay");

    ok = eglInitialize(egldisplay, NULL, NULL);
    CHECK(ok, "Can't eglInitialize");

    ok = eglChooseConfig(egldisplay, attribs, &eglconfig, 1, &neglconfigs);
    CHECK(ok, "Could not eglChooseConfig");
    CHECK(neglconfigs > 0, "No EGL config available");

    eglsurface = eglCreateWindowSurface(egldisplay, eglconfig,
            (EGLNativeWindowType)mir_surface_get_egl_native_window(surface),
            NULL);
    CHECK(eglsurface != EGL_NO_SURFACE, "eglCreateWindowSurface failed");

    eglctx = eglCreateContext(egldisplay, eglconfig, EGL_NO_CONTEXT,
                              ctxattribs);
    CHECK(eglctx != EGL_NO_CONTEXT, "eglCreateContext failed");

    ok = eglMakeCurrent(egldisplay, eglsurface, eglsurface, eglctx);
    CHECK(ok, "Can't eglMakeCurrent");

    signal(SIGINT, shutdown);
    signal(SIGTERM, shutdown);
    signal(SIGHUP, shutdown);

    printf("Surface %d DPI\n", mir_surface_get_dpi(surface));
    eglSwapInterval(egldisplay, swapinterval);

    running = 1;

    return 1;
}

struct MirConnection* mir_eglapp_native_connection()
{
    return connection;
}

struct MirSurface* mir_eglapp_native_surface()
{
    return surface;
}
