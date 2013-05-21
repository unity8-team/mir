
#include "mir_app.h"
#include "mir_toolkit/mir_client_library.h"

static const char appname[] = "egldemo";

static MirConnection *connection;

void kvant_mir_shutdown(void)
{
    mir_connection_release(connection);
    connection = NULL;
}

void kvant_mir_connect(EGLNativeDisplayType *display, EGLNativeWindowType* window, int width, int height)
{
    MirSurfaceParameters surfaceparm =
    {
        "eglappsurface",
        width, height,
        mir_pixel_format_abgr_8888,
        mir_buffer_usage_hardware
    };

    MirSurface *mir_surface;
    connection = mir_connect_sync(NULL, appname);
    mir_surface = mir_connection_create_surface_sync(connection, &surfaceparm);
    *window = (EGLNativeWindowType)mir_surface_get_egl_native_window(mir_surface);
    *display = mir_connection_get_egl_native_display(connection);
}
