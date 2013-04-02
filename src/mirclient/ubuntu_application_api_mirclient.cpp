/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include <ubuntu/application/ui/ubuntu_application_ui.h>
#include <mir_toolkit/mir_client_library.h>

#include <assert.h>

namespace
{
    char const* const socket_file = "/tmp/mir_socket";
    MirConnection *global_connection = NULL;
}
    
void
ubuntu_application_ui_init(int argc, char**argv)
{
}

StageHint
ubuntu_application_ui_setup_get_stage_hint()
{
    // TODO: Implement ~racarr
    return MAIN_STAGE_HINT;
}

FormFactorHint
ubuntu_application_ui_setup_get_form_factor_hint()
{
    // TODO: Implement ~racarr
    return DESKTOP_FORM_FACTOR_HINT;
}

void
ubuntu_application_ui_start_a_new_session(SessionCredentials* creds)
{
    if (global_connection)
        return;
    global_connection = mir_connect_sync(socket_file, creds->application_name);
    assert(global_connection);
}

EGLNativeDisplayType
ubuntu_application_ui_get_native_display()
{
    // TODO: Hack to accomodate qtubuntu ~racarr
    if (!global_connection)
        global_connection = mir_connect_sync(socket_file, "test");
    return (EGLNativeDisplayType)mir_connection_get_egl_native_display(global_connection);
}

void
ubuntu_application_ui_set_clipboard_content(void* data, size_t size)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_get_clipboard_content(void** data, size_t* size)
{
    *size = 0;
    *data = NULL;
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_create_display_info(ubuntu_application_ui_physical_display_info* info, size_t index)
{
    // TODO: Implement ~racarr
    // Noop for mirclient?
}

void
ubuntu_application_ui_destroy_display_info(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Implement ~racarr
    // Noop for mirclient?
}

int32_t
ubuntu_application_ui_query_horizontal_resolution(ubuntu_application_ui_physical_display_info info)
{
    MirDisplayInfo display_info;

    assert(global_connection);

    mir_connection_get_display_info(global_connection, &display_info);
    return (int32_t)display_info.width;
}

int32_t
ubuntu_application_ui_query_vertical_resolution(ubuntu_application_ui_physical_display_info info)
{
    MirDisplayInfo display_info;
    
    assert(global_connection);

    mir_connection_get_display_info(global_connection, &display_info);
    return (int32_t)display_info.height;
}

float
ubuntu_application_ui_query_horizontal_dpi(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Implement ~racarr
    return (float)0.0;
}

float
ubuntu_application_ui_query_vertical_dpi(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Implement ~racarr
    return (float)0.0;
}

namespace
{
struct EventContext
{
    input_event_cb cb;
    void* ctx;
};

static void ubuntu_application_ui_mir_handle_event(MirSurface* surface, MirEvent* ev, void* context)
{
    EventContext *evctx = static_cast<EventContext*>(context);
    evctx->cb(evctx->ctx, (Event*)ev); // TODO: Proper event translation ~racarr
}
}

void
ubuntu_application_ui_create_surface(ubuntu_application_ui_surface* out_surface,
                                     const char* title,
                                     int width,
                                     int height,
                                     SurfaceRole role,
                                     uint32_t flags,
                                     input_event_cb cb,
                                     void* ctx)
{
    assert(global_connection);
    MirDisplayInfo display_info;
    mir_connection_get_display_info(global_connection, &display_info);
    MirSurfaceParameters params = { title, width, height, display_info.supported_pixel_format[0], mir_buffer_usage_hardware};
    EventContext *delegate_ctx = new EventContext; // TODO: Fix leak ~racarr
    delegate_ctx->cb = cb;
    delegate_ctx->ctx = ctx;
    MirEventDelegate event_delegate = { ubuntu_application_ui_mir_handle_event, delegate_ctx };
    MirSurface* surface = mir_surface_create_sync(global_connection, &params, &event_delegate);
    assert(surface);
    // TODO: Implement ~racarr
    *out_surface = static_cast<ubuntu_application_ui_surface>(surface);
}

void
ubuntu_application_ui_request_fullscreen_for_surface(ubuntu_application_ui_surface surface)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_destroy_surface(ubuntu_application_ui_surface surface)
{
    MirSurface *mir_surface = static_cast<MirSurface*>(surface);
    mir_surface_release_sync(mir_surface);
}

EGLNativeWindowType
ubuntu_application_ui_surface_to_native_window_type(ubuntu_application_ui_surface surface)
{
    return reinterpret_cast<EGLNativeWindowType>(mir_surface_get_egl_native_window(static_cast<MirSurface*>(surface)));
}

void
ubuntu_application_ui_show_surface(ubuntu_application_ui_surface surface)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_hide_surface(ubuntu_application_ui_surface surface)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_move_surface_to(ubuntu_application_ui_surface surface,
                                      int x, int y)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_resize_surface_to(ubuntu_application_ui_surface surface,
                                        int w, int h)
{
    // TODO: Implement ~racarr
}
