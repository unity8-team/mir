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

#include "ubuntu_application_api_mircommon.h"

// C APIs
#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/ui/window.h>
#include <ubuntu/application/ui/options.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/clipboard.h>
#include <ubuntu/application/ui/display.h>
#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

#include <mir_toolkit/mir_client_library.h>

#include <stdlib.h>
#include <assert.h>

namespace
{

struct MirInputContext
{
    UAUiWindowInputEventCb cb;
    void *ctx;
};

static void
ua_ui_window_mir_handle_event(MirSurface* surface, MirEvent const* mir_ev, void* ctx)
{
    // TODO: Perhaps this indicates that mirclient library should not contain a surface argument here.
    (void) surface;
    Event ubuntu_ev;
    mir_event_to_ubuntu_event(mir_ev, ubuntu_ev);
    auto mir_ctx = static_cast<MirInputContext*>(ctx);
    mir_ctx->cb(ctx, &ubuntu_ev);
}

}

namespace
{

// Application Instance
struct MirApplicationInstance
{
    MirConnection *connection;
};
static MirApplicationInstance*
global_mir_instance()
{
    static MirApplicationInstance instance{NULL};
    return &instance;
}
static MirApplicationInstance*
assert_global_mir_instance()
{
    auto instance = global_mir_instance();
    assert(mir_connection_is_valid(instance->connection));
    return instance;
}
static UApplicationInstance*
mir_application_u_application(MirApplicationInstance *instance)
{
    return (UApplicationInstance *)instance;
}
static MirApplicationInstance*
u_application_mir_application(UApplicationInstance *instance)
{
    return (MirApplicationInstance *)instance;
}


// Display info
static MirDisplayInfo*
u_display_mir_display(UAUiDisplay *display)
{
    return (MirDisplayInfo *)display;
}

static UAUiDisplay*
mir_display_u_display(MirDisplayInfo *display)
{
    return (UAUiDisplay *)display;
}

// Window properties
struct MirWindowProperties
{
    MirSurfaceParameters parameters;
    MirEventDelegate delegate;
};
static MirWindowProperties*
u_window_properties_mir_window_properties(UAUiWindowProperties *properties)
{
    return (MirWindowProperties *)properties;
}

static UAUiWindowProperties*
mir_window_properties_u_window_properties(MirWindowProperties *properties)
{
    return (UAUiWindowProperties *)properties;
}

static MirSurface*
u_window_mir_window(UAUiWindow *window)
{
    return (MirSurface *)window;
}

static UAUiWindow*
mir_window_u_window(MirSurface *window)
{
    return (UAUiWindow *)window;
}

};

extern "C"
{

// TODO: Application description/options
// TODO: Eliminate global instance by adding Instance to some functions (i.e. display queries)
UApplicationInstance* u_application_instance_new_from_description_with_options(UApplicationDescription* description, UApplicationOptions* options)
{
    auto instance = global_mir_instance();
    // TODO: App name
    instance->connection = mir_connect_sync(NULL, "TODO: App Name");
    assert(instance->connection);
    return mir_application_u_application(instance);
}

void ua_ui_set_clipboard_content(void* content, size_t content_size)
{
    // TODO: Implement
    (void) content;
    (void) content_size;
}

void ua_ui_get_clipboard_content(void** out_content, size_t* out_content_size)
{
    // TODO: Implement
    *out_content = NULL;
    *out_content_size = 0;
}

// TODO: We need to expose the EGLNativeDisplay somehow
UAUiDisplay* ua_ui_display_new_with_index(size_t index)
{
    // TODO: Perhaps this should be noop for Mir as there is no need to construct
    // a display object.
    // TODO: Make use of index.
    // TODO: This function should take an application instance so we can eliminate the global mir instance.
    (void) index;
    auto instance = assert_global_mir_instance();

    auto display = new MirDisplayInfo;
    mir_connection_get_display_info(instance->connection, display);

    return mir_display_u_display(display);
}

void ua_ui_display_destroy(UAUiDisplay* display)
{
    auto mir_display = u_display_mir_display(display);
    delete mir_display;
}

uint32_t ua_ui_display_query_horizontal_res(UAUiDisplay* display)
{
    auto mir_display = u_display_mir_display(display);
    // TODO: Line up return types
    return (uint32_t)mir_display->width;
}

uint32_t ua_ui_display_query_vertical_res(UAUiDisplay* display)
{
    auto mir_display = u_display_mir_display(display);
    // TODO: Line up return types.
    return (uint32_t)mir_display->height;
}

UAUiWindowProperties* ua_ui_window_properties_new_for_normal_window()
{
    auto properties = new MirWindowProperties;

    auto &parameters = properties->parameters;
    parameters.name = NULL;
    // If unspecified we leave the decision up to the server
    parameters.width = 0;
    parameters.height = 0;
    parameters.pixel_format = mir_pixel_format_argb_8888; // TODO: Choose
    parameters.buffer_usage = mir_buffer_usage_hardware;
    
    auto &delegate = properties->delegate;
    delegate.callback = NULL;
    delegate.context = NULL;

    return mir_window_properties_u_window_properties(properties);
}

void ua_ui_window_properties_destroy(UAUiWindowProperties* properties)
{
    auto mir_properties = u_window_properties_mir_window_properties(properties);

    // TODO: This should be managed somehow...
    auto input_context = static_cast<MirInputContext*>(mir_properties->delegate.context);
    delete input_context;
    
    delete mir_properties;
}

void ua_ui_window_properties_set_titlen(UAUiWindowProperties* properties, const char* title, size_t title_length)
{
    // TODO: Who owns title? Why title length?
    auto mir_properties = u_window_properties_mir_window_properties(properties);
    mir_properties->parameters.name = title;
    
}

const char* ua_ui_window_properties_get_title(UAUiWindowProperties* properties)
{
    auto mir_properties = u_window_properties_mir_window_properties(properties);
    return mir_properties->parameters.name;
}

void ua_ui_window_properties_set_role(UAUiWindowProperties* properties, UAUiWindowRole role)
{
    // TODO: Does this do anything for mirclient?
    (void) properties;
    (void) role;
}

void ua_ui_window_propperties_set_input_cb_and_ctx(UAUiWindowProperties* properties, UAUiWindowInputEventCb cb, void* ctx)
{
    // Do the properties or the window itself own this?
    auto context = new MirInputContext;
    context->cb = cb;
    context->ctx = ctx;

    auto mir_properties = u_window_properties_mir_window_properties(properties);
    auto& delegate = mir_properties->delegate;
    delegate.context = context;
    delegate.callback  = ua_ui_window_mir_handle_event;
}

UAUiWindow* ua_ui_window_new_for_application_with_properties(UApplicationInstance* instance, UAUiWindowProperties* properties)
{
    auto mir_application = u_application_mir_application(instance);

    auto mir_properties = u_window_properties_mir_window_properties(properties);
    auto window = mir_connection_create_surface_sync(mir_application->connection, &mir_properties->parameters);
    mir_surface_set_event_handler(window, &mir_properties->delegate);
    return mir_window_u_window(window);
}

void ua_ui_window_destroy(UAUiWindow* window)
{
    auto mir_window = u_window_mir_window(window);
    mir_surface_release_sync(mir_window);
}

UStatus ua_ui_window_move(UAUiWindow* window, uint32_t x, uint32_t y)
{
    // TODO: Implement. Assuming this should exist on mirclient?
    (void) window;
    (void) x;
    (void) y;
    return (UStatus) 0;
}

UStatus ua_ui_window_resize(UAUiWindow* window, uint32_t width, uint32_t height)
{
    // TODO: Implement
    (void) window;
    (void) width;
    (void) height;
    return (UStatus) 0;
}

UStatus ua_ui_window_hide(UAUiWindow* window)
{
    // TODO: Implement
    (void) window;
    return (UStatus) 0;
}

UStatus ua_ui_window_show(UAUiWindow* window)
{
    // TODO: Implement
    (void) window;
    return (UStatus) 0;
}

void ua_ui_window_request_fullscreen(UAUiWindow* window)
{
    // TODO: Implement
    (void) window;
}

EGLNativeWindowType ua_ui_window_get_native_type(UAUiWindow* window)
{
    auto mir_window = u_window_mir_window(window);
    // TODO: Careful with this cast!
    return reinterpret_cast<EGLNativeWindowType>(mir_surface_get_egl_native_window(mir_window));
}

// TODO: Sensors
}
