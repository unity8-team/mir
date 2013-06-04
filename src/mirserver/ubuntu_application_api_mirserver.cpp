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

#include "ubuntu_application_api_mirserver_priv.h"
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

#include <mir/default_server_configuration.h>

#include <mir/graphics/display.h>
#include <mir/graphics/platform.h>
#include <mir/graphics/internal_client.h>
#include <mir/frontend/session.h>
#include <mir/frontend/shell.h>
#include <mir/shell/surface_creation_parameters.h>
#include <mir/shell/surface.h>
#include <mir/input/input_platform.h>
#include <mir/input/input_receiver_thread.h>


#include <assert.h>

#include <memory>

namespace
{

struct MirServerContext
{
    std::shared_ptr<mir::graphics::Display> display;
    std::shared_ptr<mir::frontend::Shell> shell;
    std::shared_ptr<mir::input::receiver::InputPlatform> input_platform;
    std::shared_ptr<mir::graphics::InternalClient> egl_client;
};

MirServerContext *
global_mirserver_context()
{
    static MirServerContext context;
    return &context;
}

void ua_ui_mirserver_init(mir::DefaultServerConfiguration& config)
{
    auto context = global_mirserver_context();

    context->display = config.the_display();
    context->shell = config.the_frontend_shell();
    context->input_platform = mir::input::receiver::InputPlatform::create();
    context->egl_client = config.the_graphics_platform()->create_internal_client();
}

void ua_ui_mirserver_finish()
{
    auto context = global_mirserver_context();

    context->display.reset();
    context->shell.reset();
    context->input_platform.reset();
    context->egl_client.reset();
}

// Application instance
struct MirServerApplicationInstance
{
    MirServerApplicationInstance()
        : ref_count(1)
    {
    }

    std::shared_ptr<mir::frontend::Session> session;
    int ref_count;
};

MirServerApplicationInstance *
u_application_mirserver_application(UApplicationInstance *instance)
{
    return static_cast<MirServerApplicationInstance*>(instance);
}
UApplicationInstance*
mirserver_application_u_application(MirServerApplicationInstance *instance)
{
    return static_cast<UApplicationInstance*>(instance);
}


// Window properties
struct MirServerWindowProperties
{
    MirServerWindowProperties()
        : parameters(mir::shell::a_surface()),
          cb(0),
          ctx(0)
    {
    }
    mir::shell::SurfaceCreationParameters parameters;
    UAUiWindowInputEventCb cb;
    void* ctx;
};
MirServerWindowProperties*
u_window_properties_mirserver_window_properties(UAUiWindowProperties *properties)
{
    return static_cast<MirServerWindowProperties*>(properties);
}
UAUiWindowProperties*
mirserver_window_properties_u_window_properties(MirServerWindowProperties *properties)
{
    return static_cast<UAUiWindowProperties*>(properties);
}

// Window
struct MirServerWindow
{
    std::shared_ptr<mir::shell::Surface> surface;
    std::shared_ptr<mir::input::receiver::InputReceiverThread> input_thread;
};
MirServerWindow*
u_window_mirserver_window(UAUiWindow* window)
{
    return static_cast<MirServerWindow*>(window);
}
UAUiWindow*
mirserver_window_u_window(MirServerWindow* window)
{
    return static_cast<UAUiWindow*>(window);
}

}


extern "C"
{
// TODO: Application description/options
UApplicationInstance* u_application_instance_new_from_description_with_options(UApplicationDescription* description, UApplicationOptions* options)
{
    auto shell = global_mirserver_context()->shell;
    assert(shell);

    // TODO: Make use of descriptions and options
    auto instance = new MirServerApplicationInstance;

    instance->session = shell->open_session(std::string("TODO: Name"),
        std::shared_ptr<mir::events::EventSink>());

    return mirserver_application_u_application(instance);
}

void
u_application_instance_ref(UApplicationInstance *instance)
{
    auto mir_instance = u_application_mirserver_application(instance);
    mir_instance->ref_count++;
}
    
void
u_application_instance_unref(UApplicationInstance *instance)
{
    auto mir_instance = u_application_mirserver_application(instance);
    mir_instance->ref_count--;
    if (mir_instance->ref_count == 0)
        delete mir_instance;
}
    
void
u_application_instance_destroy(UApplicationInstance *instance)
{
    // TODO: What are the proper semantics here.
    u_application_instance_unref(instance);
}
    
void
u_application_instance_run(UApplicationInstance *instance)
{
    // TODO: What is this supposed to do? Seems to be no-op on hybris.
    (void) instance;
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

UAUiDisplay* ua_ui_display_new_with_index(size_t index)
{
    // TODO: Make use of index. This is kind of strangely done...
    return reinterpret_cast<UAUiDisplay*>(index);
}

void ua_ui_display_destroy(UAUiDisplay* display)
{
    // TODO: Implement. Or is this a noop for mirserver?
    (void) display;
}

uint32_t ua_ui_display_query_horizontal_res(UAUiDisplay* display)
{
    (void) display; // TODO: Multiple displays

    auto mir_display = global_mirserver_context()->display;
    assert(mir_display);
    
    return mir_display->view_area().size.width.as_uint32_t();
}

uint32_t ua_ui_display_query_vertical_res(UAUiDisplay* display)
{
    (void) display; // TODO: Multiple displays

    auto mir_display = global_mirserver_context()->display;
    assert(mir_display);
    
    return mir_display->view_area().size.height.as_uint32_t();
}

UAUiWindowProperties* ua_ui_window_properties_new_for_normal_window()
{
    auto properties = new MirServerWindowProperties;
    // TODO: We need to choose the pixel format on the surface parameters
    return mirserver_window_properties_u_window_properties(properties);
}

void ua_ui_window_properties_destroy(UAUiWindowProperties* properties)
{
    auto mir_properties = u_window_properties_mirserver_window_properties(properties);
    delete mir_properties;
}

void ua_ui_window_properties_set_titlen(UAUiWindowProperties* properties, const char* title, size_t title_length)
{
    auto mir_properties = u_window_properties_mirserver_window_properties(properties);
    mir_properties->parameters = mir_properties->parameters.of_name(std::string(title, title_length));
}

const char* ua_ui_window_properties_get_title(UAUiWindowProperties* properties)
{
    auto mir_properties = u_window_properties_mirserver_window_properties(properties);
    return mir_properties->parameters.name.c_str();
}

void ua_ui_window_properties_set_role(UAUiWindowProperties* properties, UAUiWindowRole role)
{
    // TODO: Implement. Or is this a noop for us?
    (void) properties;
    (void) role;
}

void ua_ui_window_properties_set_input_cb_and_ctx(UAUiWindowProperties* properties, UAUiWindowInputEventCb cb, void* ctx)
{
    auto mir_properties = u_window_properties_mirserver_window_properties(properties);
    mir_properties->cb = cb;
    mir_properties->ctx = ctx;
}

namespace
{

static void ua_ui_window_handle_event(UAUiWindowInputEventCb cb, void* ctx, MirEvent* mir_event)
{
    Event ubuntu_ev;
    mir_event_to_ubuntu_event(mir_event, ubuntu_ev);
    cb(ctx, &ubuntu_ev);
}

}

UAUiWindow* ua_ui_window_new_for_application_with_properties(UApplicationInstance* instance, UAUiWindowProperties* properties)
{
    auto shell = global_mirserver_context()->shell;
    assert(shell);
    auto input_platform = global_mirserver_context()->input_platform;
    assert(input_platform);

    auto mir_instance = u_application_mirserver_application(instance);
    auto mir_properties = u_window_properties_mirserver_window_properties(properties);

    auto window = new MirServerWindow;
    window->surface = std::dynamic_pointer_cast<mir::shell::Surface>(mir_instance->session->get_surface(
        shell->create_surface_for(mir_instance->session, mir_properties->parameters)));
    window->input_thread = input_platform->create_input_thread(window->surface->client_input_fd(),
        std::bind(ua_ui_window_handle_event, mir_properties->cb, mir_properties->ctx, std::placeholders::_1));
    window->input_thread->start();

    // TODO: Verify that we don't have to advance the client buffer anymore ~racarr

    return mirserver_window_u_window(window);
}

void ua_ui_window_destroy(UAUiWindow* window)
{
    auto mir_window = u_window_mirserver_window(window);
    mir_window->input_thread->stop();
    mir_window->input_thread->join();

    // TODO: Is this enough to ensure we don't leak the surface, or should we close it through the session?
    delete mir_window;
}

UStatus ua_ui_window_move(UAUiWindow* window, uint32_t x, uint32_t y)
{
    // TODO: Implement
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
    auto egl_client = global_mirserver_context()->egl_client;
    auto mir_window = u_window_mirserver_window(window);

    return egl_client->egl_native_window(mir_window->surface);
}

// TODO: Sensors
}
