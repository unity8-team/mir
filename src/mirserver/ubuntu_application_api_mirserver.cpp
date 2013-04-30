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

#include <mir_toolkit/event.h>

#include <mirserver/mir/display_server.h>
#include <mirserver/mir/shell/session_manager.h>
#include <mirserver/mir/graphics/platform.h>

#include <assert.h>

namespace msh = mir::shell;
namespace mg = mir::graphics;

namespace
{
// TODO Leaks
typedef struct {
    std::shared_ptr<msh::SessionManager> session_manager;
    std::shared_ptr<mg::Platform> graphics_platform;
} ubuntu_application_ui_mirserver_context;

ubuntu_application_ui_mirserver_context global_context;

extern "C"
{

void ubuntu_application_ui_mirserver_init(
    std::shared_ptr<msh::SessionManager> const& session_manager, 
    std::shared_ptr<mg::Platform> const& graphics_platform)
{
    global_context.session_manager = session_manager;
    global_context.graphics_platform = graphics_platform;
}

}

}

typedef struct {
    void *context;
} ubuntu_application_ui_mir_surface;

    
void
ubuntu_application_ui_init(int argc, char**argv)
{
    assert(global_context.session_manager);
    assert(global_context.graphics_platform);
}

StageHint
ubuntu_application_ui_setup_get_stage_hint()
{
    return MAIN_STAGE_HINT;
}

FormFactorHint
ubuntu_application_ui_setup_get_form_factor_hint()
{
    return DESKTOP_FORM_FACTOR_HINT;
}

void
ubuntu_application_ui_start_a_new_session(SessionCredentials* creds)
{
    (void) creds;
}

EGLNativeDisplayType
ubuntu_application_ui_get_native_display()
{
    
}

void
ubuntu_application_ui_set_clipboard_content(void* data, size_t size)
{
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
}

void
ubuntu_application_ui_destroy_display_info(ubuntu_application_ui_physical_display_info info)
{
}

int32_t
ubuntu_application_ui_query_horizontal_resolution(ubuntu_application_ui_physical_display_info info)
{
}

int32_t
ubuntu_application_ui_query_vertical_resolution(ubuntu_application_ui_physical_display_info info)
{
}

float
ubuntu_application_ui_query_horizontal_dpi(ubuntu_application_ui_physical_display_info info)
{
}

float
ubuntu_application_ui_query_vertical_dpi(ubuntu_application_ui_physical_display_info info)
{
}

namespace
{

static void
mir_event_to_ubuntu_event(MirEvent const* mir_event, Event& ubuntu_ev)
{
    switch (mir_event->type)
    {
    case mir_event_type_key:
        ubuntu_ev.type = KEY_EVENT_TYPE;
        ubuntu_ev.device_id = mir_event->key.device_id;
        ubuntu_ev.source_id = mir_event->key.source_id;
        ubuntu_ev.action = mir_event->key.action;
        ubuntu_ev.flags = mir_event->key.flags;
        ubuntu_ev.meta_state = mir_event->key.meta_state;
        ubuntu_ev.details.key.key_code = mir_event->key.key_code;
        ubuntu_ev.details.key.scan_code = mir_event->key.scan_code;
        ubuntu_ev.details.key.repeat_count = mir_event->key.repeat_count;
        ubuntu_ev.details.key.down_time = mir_event->key.down_time;
        ubuntu_ev.details.key.event_time = mir_event->key.event_time;
        ubuntu_ev.details.key.is_system_key = mir_event->key.is_system_key;
        break;
    case mir_event_type_motion:
        ubuntu_ev.type = MOTION_EVENT_TYPE;
        ubuntu_ev.device_id = mir_event->motion.device_id;
        ubuntu_ev.source_id = mir_event->motion.source_id;
        ubuntu_ev.action = mir_event->motion.action;
        ubuntu_ev.flags = mir_event->motion.flags;
        ubuntu_ev.meta_state = mir_event->motion.meta_state;
        ubuntu_ev.details.motion.edge_flags = mir_event->motion.edge_flags;
        ubuntu_ev.details.motion.button_state = mir_event->motion.button_state;
        ubuntu_ev.details.motion.x_offset = mir_event->motion.x_offset;
        ubuntu_ev.details.motion.y_offset = mir_event->motion.y_offset;
        ubuntu_ev.details.motion.x_precision = mir_event->motion.x_precision;
        ubuntu_ev.details.motion.y_precision = mir_event->motion.y_precision;
        ubuntu_ev.details.motion.down_time = mir_event->motion.down_time;
        ubuntu_ev.details.motion.event_time = mir_event->motion.event_time;
        ubuntu_ev.details.motion.pointer_count = mir_event->motion.pointer_count;
        for (int i = 0; i < mir_event->motion.pointer_count; i++)
        {
            ubuntu_ev.details.motion.pointer_coordinates[i].id = mir_event->motion.pointer_coordinates[i].id;
            ubuntu_ev.details.motion.pointer_coordinates[i].x = mir_event->motion.pointer_coordinates[i].x;
            ubuntu_ev.details.motion.pointer_coordinates[i].raw_x = mir_event->motion.pointer_coordinates[i].raw_x;
            ubuntu_ev.details.motion.pointer_coordinates[i].y = mir_event->motion.pointer_coordinates[i].y;
            ubuntu_ev.details.motion.pointer_coordinates[i].raw_y = mir_event->motion.pointer_coordinates[i].raw_y;
            ubuntu_ev.details.motion.pointer_coordinates[i].touch_major = mir_event->motion.pointer_coordinates[i].touch_major;
            ubuntu_ev.details.motion.pointer_coordinates[i].touch_minor = mir_event->motion.pointer_coordinates[i].touch_minor;
            ubuntu_ev.details.motion.pointer_coordinates[i].size = mir_event->motion.pointer_coordinates[i].size;
            ubuntu_ev.details.motion.pointer_coordinates[i].pressure = mir_event->motion.pointer_coordinates[i].pressure;
            ubuntu_ev.details.motion.pointer_coordinates[i].orientation = mir_event->motion.pointer_coordinates[i].orientation;
        }
        break;
    default:
        break;
    }
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
}

void
ubuntu_application_ui_request_fullscreen_for_surface(ubuntu_application_ui_surface surface)
{
}

void
ubuntu_application_ui_destroy_surface(ubuntu_application_ui_surface s)
{
}

EGLNativeWindowType
ubuntu_application_ui_surface_to_native_window_type(ubuntu_application_ui_surface s)
{
}

void
ubuntu_application_ui_show_surface(ubuntu_application_ui_surface surface)
{
}

void
ubuntu_application_ui_hide_surface(ubuntu_application_ui_surface surface)
{
}

void
ubuntu_application_ui_move_surface_to(ubuntu_application_ui_surface surface,
                                      int x, int y)
{
}

void
ubuntu_application_ui_resize_surface_to(ubuntu_application_ui_surface surface,
                                        int w, int h)
{
}
