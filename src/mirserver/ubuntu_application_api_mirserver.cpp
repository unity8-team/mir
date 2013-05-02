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
#include <mirserver/mir/frontend/session.h>
#include <mirserver/mir/frontend/surface_creation_parameters.h>
#include <mirserver/mir/shell/surface.h>
#include <mirserver/mir/graphics/platform.h>
#include <mirserver/mir/graphics/display.h>
#include <mircommon/mir/input/input_platform.h>
#include <mircommon/mir/input/input_receiver_thread.h>

#include <assert.h>

#include <functional>

namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace me = mir::events;
namespace mf = mir::frontend; // TODO: Shouldn't be used
namespace mcli = mir::client::input;
namespace geom = mir::geometry;

// TODO: Fix GLM includes

namespace
{
// TODO Leaks
typedef struct {
    std::shared_ptr<msh::SessionManager> session_manager;
    std::shared_ptr<mg::Platform> graphics_platform;
    std::shared_ptr<mg::Display> display;
    std::shared_ptr<mcli::InputPlatform> input_platform;
} ubuntu_application_ui_mirserver_context;


typedef struct {
    std::shared_ptr<mf::Session> session; // TODO: Split
    std::shared_ptr<msh::Surface> surface;
    
    std::shared_ptr<mcli::InputReceiverThread> input_thread;
    input_event_cb input_cb;
    void *input_ctx;
} ubuntu_application_ui_mirserver_surface;

ubuntu_application_ui_mirserver_context global_context;

static ubuntu_application_ui_mirserver_surface* ui_surf_mir_surf(ubuntu_application_ui_surface ui_surf)
{
    return static_cast<ubuntu_application_ui_mirserver_surface*>(ui_surf);
}

extern "C"
{

void ubuntu_application_ui_mirserver_init(
    std::shared_ptr<msh::SessionManager> const& session_manager, 
    std::shared_ptr<mg::Platform> const& graphics_platform,
    std::shared_ptr<mg::Display> const& display)
{
    global_context.session_manager = session_manager;
    global_context.graphics_platform = graphics_platform;
    global_context.display = display;
    global_context.input_platform = mcli::InputPlatform::create();
}

}

}
    
void
ubuntu_application_ui_init(int argc, char**argv)
{
    assert(global_context.session_manager);
    assert(global_context.graphics_platform);
    assert(global_context.display);
    assert(global_context.input_platform);
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
    return global_context.graphics_platform->shell_egl_display();
}

void
ubuntu_application_ui_set_clipboard_content(void* data, size_t size)
{
    // TODO
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
    // TODO: Noop?
}

void
ubuntu_application_ui_destroy_display_info(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Noop?
}

int32_t
ubuntu_application_ui_query_horizontal_resolution(ubuntu_application_ui_physical_display_info info)
{
    return static_cast<int32_t>(global_context.display->view_area().size.width.as_uint32_t());
}

int32_t
ubuntu_application_ui_query_vertical_resolution(ubuntu_application_ui_physical_display_info info)
{
    return static_cast<int32_t>(global_context.display->view_area().size.height.as_uint32_t());
}

float
ubuntu_application_ui_query_horizontal_dpi(ubuntu_application_ui_physical_display_info info)
{
    // TODO
    return 90.0;
}

float
ubuntu_application_ui_query_vertical_dpi(ubuntu_application_ui_physical_display_info info)
{
    // TODO
    return 90.0;
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
        ubuntu_ev.modifiers = mir_event->key.modifiers;
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
        ubuntu_ev.modifiers = mir_event->motion.modifiers;
        ubuntu_ev.details.motion.edge_flags = mir_event->motion.edge_flags;
        ubuntu_ev.details.motion.button_state = mir_event->motion.button_state;
        ubuntu_ev.details.motion.x_offset = mir_event->motion.x_offset;
        ubuntu_ev.details.motion.y_offset = mir_event->motion.y_offset;
        ubuntu_ev.details.motion.x_precision = mir_event->motion.x_precision;
        ubuntu_ev.details.motion.y_precision = mir_event->motion.y_precision;
        ubuntu_ev.details.motion.down_time = mir_event->motion.down_time;
        ubuntu_ev.details.motion.event_time = mir_event->motion.event_time;
        ubuntu_ev.details.motion.pointer_count = mir_event->motion.pointer_count;
        for (uint i = 0; i < mir_event->motion.pointer_count; i++)
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

static void handle_event(ubuntu_application_ui_mirserver_surface *surface,
                         MirEvent * mir_event)
{
    Event ubuntu_ev;
    mir_event_to_ubuntu_event(mir_event, ubuntu_ev);
    
    surface->input_cb(surface->input_ctx, &ubuntu_ev);
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
    // TODO: Obviously wrong place for this
    auto session = global_context.session_manager->open_session(title, std::shared_ptr<me::EventSink>());
    
    auto surface_params = mf::a_surface().of_name(title)
        .of_size(geom::Size{geom::Width{width},
                            geom::Height{height}})
        .of_pixel_format(mir::geometry::PixelFormat::argb_8888);
    // TODO: Nasty cast
    auto surface = std::dynamic_pointer_cast<msh::Surface>(session->get_surface(global_context.session_manager->create_surface_for(session, surface_params)));

    // TODO: Wart
    surface->advance_client_buffer();

    ubuntu_application_ui_mirserver_surface *ui_surf = new ubuntu_application_ui_mirserver_surface;
    ui_surf->session = session;
    ui_surf->surface = surface;
    ui_surf->input_cb = cb;
    ui_surf->input_ctx = ctx;
    *out_surface = ui_surf;
    
    ui_surf->input_thread = global_context.input_platform->create_input_thread(surface->client_input_fd(), std::bind(handle_event, ui_surf, std::placeholders::_1));
    ui_surf->input_thread->start();
    
    (void) role; (void) flags;
}

void
ubuntu_application_ui_request_fullscreen_for_surface(ubuntu_application_ui_surface surface)
{
}

void
ubuntu_application_ui_destroy_surface(ubuntu_application_ui_surface s)
{
    auto surf = ui_surf_mir_surf(s);
    surf->input_thread->stop();
    surf->input_thread->join();
    global_context.session_manager->close_session(surf->session);
    delete surf;
}

EGLNativeWindowType
ubuntu_application_ui_surface_to_native_window_type(ubuntu_application_ui_surface s)
{
    auto ui_surf = ui_surf_mir_surf(s);
    auto frontend_surf = std::dynamic_pointer_cast<mf::Surface>(ui_surf->surface);
    assert(frontend_surf.get());
    return reinterpret_cast<EGLNativeWindowType>(frontend_surf.get());
}

void
ubuntu_application_ui_show_surface(ubuntu_application_ui_surface surface)
{
    auto ui_surf = ui_surf_mir_surf(surface);
    ui_surf->surface->show();
}

void
ubuntu_application_ui_hide_surface(ubuntu_application_ui_surface surface)
{
    auto ui_surf = ui_surf_mir_surf(surface);
    ui_surf->surface->hide();
}

void
ubuntu_application_ui_move_surface_to(ubuntu_application_ui_surface surface,
                                      int x, int y)
{
    // TODO: Implement
}

void
ubuntu_application_ui_resize_surface_to(ubuntu_application_ui_surface surface,
                                        int w, int h)
{
    // .TODO: Implement
}
