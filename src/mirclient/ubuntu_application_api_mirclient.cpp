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

#include "mircommon/event_helpers_mir.h"
#include "mircommon/application_id_mir_priv.h"

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
#include <string.h>

namespace uam = ubuntu::application::mir;
namespace uaum = ubuntu::application::ui::mir;

namespace
{

struct MirClientInputContext
{
    UAUiWindowInputEventCb cb;
    void *ctx;
};

static void
ua_ui_window_mir_handle_event(MirSurface* surface, MirEvent const* mir_ev, void* ctx)
{
    // TODO<mir>: Perhaps this indicates that mirclient library should not pass a surface argument here.
    (void) surface;

    Event ubuntu_ev;
    uaum::event_to_ubuntu_event(mir_ev, ubuntu_ev);
    auto mir_ctx = static_cast<MirClientInputContext*>(ctx);
    mir_ctx->cb(mir_ctx->ctx, &ubuntu_ev);
}

}

namespace
{

// Application Instance
// We use a global instance as some platform-api functions, i.e. display_new_with_index
// do not supply dependencies, but a MirConnection is required for all queries.
struct MirApplicationInstance
{
    MirConnection *connection;
    int ref_count;

    // TODO<papi>: Obviously this is yucky, remove when we can.
    bool is_global_yuck;
};
static MirApplicationInstance*
global_mir_instance()
{
    // Obviously ref counting is whacky here...
    static MirApplicationInstance instance{NULL, 1, true};
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
mir_application_u_application(MirApplicationInstance* instance)
{
    return static_cast<UApplicationInstance*>(instance);
}
static MirApplicationInstance*
u_application_mir_application(UApplicationInstance* instance)
{
    return static_cast<MirApplicationInstance*>(instance);
}

// Display info
static MirDisplayInfo*
u_display_mir_display(UAUiDisplay *display)
{
    return static_cast<MirDisplayInfo*>(display);
}

static UAUiDisplay*
mir_display_u_display(MirDisplayInfo *display)
{
    return static_cast<UAUiDisplay*>(display);
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
    return static_cast<MirWindowProperties*>(properties);
}

static UAUiWindowProperties*
mir_window_properties_u_window_properties(MirWindowProperties *properties)
{
    return static_cast<UAUiWindowProperties*>(properties);
}

static MirSurface*
u_window_mir_window(UAUiWindow *window)
{
    return static_cast<MirSurface*>(window);
}

static UAUiWindow*
mir_window_u_window(MirSurface *window)
{
    return static_cast<UAUiWindow*>(window);
}

};

extern "C"
{

// TODO<papi>: Eliminate global instance by adding Instance to some functions (i.e. display queries)
UApplicationInstance* u_application_instance_new_from_description_with_options(UApplicationDescription* description, UApplicationOptions* options)
{
    // TODO<mir>: Make use of options!
    (void) options;

    auto instance = global_mir_instance();

    auto id = uam::Id::from_u_application_id(u_application_description_get_application_id(description));
    instance->connection = mir_connect_sync(NULL, id->name.c_str());
    assert(instance->connection);
    
    instance->is_global_yuck = false;

    return mir_application_u_application(instance);
}

void
u_application_instance_ref(UApplicationInstance *instance)
{
    auto mir_instance = u_application_mir_application(instance);
    mir_instance->ref_count++;
}
    
void
u_application_instance_unref(UApplicationInstance *instance)
{
    auto mir_instance = u_application_mir_application(instance);
    mir_instance->ref_count--;
    if (mir_instance->ref_count == 0 && mir_instance->is_global_yuck == false)
        delete mir_instance;
}
    
void
u_application_instance_destroy(UApplicationInstance *instance)
{
    // TODO<papi>: What are the proper semantics here.
    u_application_instance_unref(instance);
}
    
void
u_application_instance_run(UApplicationInstance *instance)
{
    // TODO<papi>: What is this supposed to do? Seems to be no-op on hybris.
    (void) instance;
}

void ua_ui_set_clipboard_content(void* content, size_t content_size)
{
    // TODO<papi,mir>: Implement. We will need a MirConnection
    (void) content;
    (void) content_size;
}

void ua_ui_get_clipboard_content(void** out_content, size_t* out_content_size)
{
    // TODO<papi,mir>: Implement, see get_clipboard_comment.
    *out_content = NULL;
    *out_content_size = 0;
}

//
// TODO <papi>: We need to expose the EGLNativeDisplay somehow
//

// TODO <papi>: This function should take an application instance so we can eliminate the global mir instance.
// TODO <mir>: Perhaps this should be noop for Mir as there is no need to construct
// a display object, the connection already has everything we need, as it receives
// the display info at connect time.
UAUiDisplay* ua_ui_display_new_with_index(size_t index)
{
    // TODO<papi,mir>: What are the semantics of index. How should we use it?
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

    // TODO<mir>: Line up return types from mirclient
    return static_cast<uint32_t>(mir_display->width);
}

uint32_t ua_ui_display_query_vertical_res(UAUiDisplay* display)
{
    auto mir_display = u_display_mir_display(display);

    // TODO<mir>: Line up return types from mirclient
    return static_cast<uint32_t>(mir_display->height);
}

EGLNativeDisplayType ua_ui_display_get_native_type(UAUiDisplay* display)
{
    auto instance = assert_global_mir_instance();

    // TODO<mir>: Careful with this cast
    return reinterpret_cast<EGLNativeDisplayType>(mir_connection_get_egl_native_display(instance->connection));
}

UAUiWindowProperties* ua_ui_window_properties_new_for_normal_window()
{
    auto properties = new MirWindowProperties;

    auto &parameters = properties->parameters;
    parameters.name = NULL;

    // If unspecified the server will choose a width and height for us.
    parameters.width = 0;
    parameters.height = 0;
    parameters.buffer_usage = mir_buffer_usage_hardware;

    // We still have to set a pixel format on the parameters. We do this at window creation
    // time as we need the MirConnection.
    
    auto &delegate = properties->delegate;
    delegate.callback = NULL;
    delegate.context = NULL;

    return mir_window_properties_u_window_properties(properties);
}

void ua_ui_window_properties_destroy(UAUiWindowProperties* properties)
{
    auto mir_properties = u_window_properties_mir_window_properties(properties);

    // TODO<mir>: This should be managed somehow...
    auto input_context = static_cast<MirClientInputContext*>(mir_properties->delegate.context);
    // TODO<papi>: Who owns this? Do we transfer ownership to the window or does the window own
    // the window properties?
    delete input_context;

    delete[] mir_properties->parameters.name;
    
    delete mir_properties;
}

void ua_ui_window_properties_set_titlen(UAUiWindowProperties* properties, const char* title, size_t title_length)
{
    auto mir_properties = u_window_properties_mir_window_properties(properties);
    
    auto name = new char[title_length+1];
    strncpy(name, title, title_length);
    name[title_length] = '\0';

    mir_properties->parameters.name = name;
}

const char* ua_ui_window_properties_get_title(UAUiWindowProperties* properties)
{
    auto mir_properties = u_window_properties_mir_window_properties(properties);
    return mir_properties->parameters.name;
}

void ua_ui_window_properties_set_role(UAUiWindowProperties* properties, UAUiWindowRole role)
{
    // TODO<papi>: Doesn't seem like this is meaningful for mirclient. Perhaps it should leave platform-api.
    (void) properties;
    (void) role;
}

void ua_ui_window_properties_set_input_cb_and_ctx(UAUiWindowProperties* properties, UAUiWindowInputEventCb cb, void* ctx)
{
    // TODO<papi>: Do the properties or the window itself own this?
    auto context = new MirClientInputContext;
    context->cb = cb;
    context->ctx = ctx;

    auto mir_properties = u_window_properties_mir_window_properties(properties);
    auto& delegate = mir_properties->delegate;
    delegate.context = context;
    delegate.callback  = ua_ui_window_mir_handle_event;
}

static MirPixelFormat
mir_choose_default_pixel_format(MirConnection *connection)
{
    MirDisplayInfo info;
    mir_connection_get_display_info(connection, &info);
    return info.supported_pixel_format[0];
}

UAUiWindow* ua_ui_window_new_for_application_with_properties(UApplicationInstance* instance, UAUiWindowProperties* properties)
{
    auto mir_application = u_application_mir_application(instance);
    auto mir_properties = u_window_properties_mir_window_properties(properties);

    mir_properties->parameters.pixel_format = mir_choose_default_pixel_format(mir_application->connection);

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
    // TODO<papi,mir>: Implement. Assuming this should exist on mirclient?
    (void) window;
    (void) x;
    (void) y;
    return (UStatus) 0;
}

UStatus ua_ui_window_resize(UAUiWindow* window, uint32_t width, uint32_t height)
{
    // TODO<mir>: Implement
    (void) window;
    (void) width;
    (void) height;
    return (UStatus) 0;
}

UStatus ua_ui_window_hide(UAUiWindow* window)
{
    // TODO<mir>: Implement
    (void) window;
    return (UStatus) 0;
}

UStatus ua_ui_window_show(UAUiWindow* window)
{
    // TODO<mir>: Implement
    (void) window;
    return (UStatus) 0;
}

void ua_ui_window_request_fullscreen(UAUiWindow* window)
{
    // TODO<mir>: Implement
    (void) window;
}

EGLNativeWindowType ua_ui_window_get_native_type(UAUiWindow* window)
{
    auto mir_window = u_window_mir_window(window);
    // TODO<mir>: Careful with this cast!
    return reinterpret_cast<EGLNativeWindowType>(mir_surface_get_egl_native_window(mir_window));
}

// TODO: Sensors
}
