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

#include "application_instance_mirclient_priv.h"
#include "window_properties_mirclient_priv.h"
#include "window_mirclient_priv.h"
#include "mircommon/lifecycle_delegate_mir_priv.h"

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
#include <mir/client/private.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>

namespace uam = ubuntu::application::mir;
namespace uamc = uam::client;

namespace
{
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

static void dispatch_callback(MirConnection* conn, MirLifecycleState state, void* context)
{
    auto delegate = static_cast<uam::LifecycleDelegate*>(context);
    void* c_ctx = u_application_lifecycle_delegate_get_context(delegate->as_u_lifecycle_delegate(), nullptr);

    switch (state)
    {
    case mir_lifecycle_state_will_suspend:
    {
        if (delegate->stop_cb)
            delegate->stop_cb(nullptr, c_ctx);
        break;
    }
    case mir_lifecycle_state_resumed:
    {
        if (delegate->resumed_cb)
            delegate->resumed_cb(nullptr, c_ctx);
        break;
    }
    default:
        break;
    }
}
};

extern "C"
{

void u_application_init(void *args)
{
    (void) args;
}

void u_application_finish()
{
}

// TODO<papi>: Eliminate global instance by adding Instance to some functions (i.e. display queries)
// TODO: We can't take ownership of description and options until we get rid of the global instance
// so we leak them.
UApplicationInstance* u_application_instance_new_from_description_with_options(UApplicationDescription* description, UApplicationOptions* options)
{
    auto instance = global_mir_instance();

    auto id = uam::Id::from_u_application_id(u_application_description_get_application_id(description));
    auto connect_succeeded = instance->connect(id->name);

    if (!connect_succeeded)
        return nullptr;

    auto delegate = u_application_description_get_application_lifecycle_delegate(description);
    mir_connection_set_lifecycle_event_callback(instance->connection(), &dispatch_callback, delegate);

    return instance->as_u_application_instance();
}

void
u_application_instance_ref(UApplicationInstance *u_instance)
{
    auto instance = uamc::Instance::from_u_application_instance(u_instance);
    instance->ref();
}
    
void
u_application_instance_unref(UApplicationInstance *u_instance)
{
    auto instance = uamc::Instance::from_u_application_instance(u_instance);
    instance->unref();
}
    
void
u_application_instance_destroy(UApplicationInstance *u_instance)
{
    // TODO<papi>: What are the proper semantics here.
    u_application_instance_unref(u_instance);
}
    
void
u_application_instance_run(UApplicationInstance *instance)
{
    // TODO<papi>: What is this supposed to do? Seems to be no-op on hybris.
    (void) instance;
}

namespace {
void got_rpc_reply_callback(uamc::Instance* instance)
{
    {
        std::unique_lock<std::mutex> rpc_lock(instance->rpc_mutex);
        instance->rpc_waiting_reply = false;
    }
    instance->rpc_condition_variable.notify_all();
}
} // anonymous namespace

void ua_ui_set_clipboard_content(void* content, size_t content_size)
{
    // TODO<papi>: We will need a MirConnection.
    //             Add a non-blocking version?

    uamc::Instance* instance = global_mir_instance();
    std::unique_lock<std::mutex> clipboard_lock(instance->clipboard_api_mutex);
    std::unique_lock<std::mutex> rpc_lock(instance->rpc_mutex);
    auto const rpc_channel = mir::client::the_rpc_channel(instance->connection());

    std::string contentString(reinterpret_cast<char*>(content), content_size);

    unity::protobuf::UnityService::Stub unityService(rpc_channel.get(),
        google::protobuf::Service::STUB_DOESNT_OWN_CHANNEL);

    instance->rpc_clip.set_content(contentString);

    instance->rpc_waiting_reply = true;
    unityService.copy(
        nullptr, /* rpc_controller */
        &instance->rpc_clip, /* request  */
        &instance->rpc_void,
        google::protobuf::NewCallback(&got_rpc_reply_callback, instance));

    // Wait until we get a reply from the server
    instance->rpc_condition_variable.wait(rpc_lock, [&]{return !instance->rpc_waiting_reply;});
}

void ua_ui_get_clipboard_content(void** out_content, size_t* out_content_size)
{
    // TODO<papi>: see get_clipboard_comment.

    uamc::Instance* instance = global_mir_instance();
    std::unique_lock<std::mutex> clipboard_lock(instance->clipboard_api_mutex);
    std::unique_lock<std::mutex> rpc_lock(instance->rpc_mutex);

    *out_content = NULL;
    *out_content_size = 0;

    auto const rpc_channel = mir::client::the_rpc_channel(instance->connection());
    unity::protobuf::UnityService::Stub unityService(rpc_channel.get(),
        google::protobuf::Service::STUB_DOESNT_OWN_CHANNEL);

    unityService.paste(
        nullptr, /* rpc_controller */
        &instance->rpc_void, /* request */
        &instance->rpc_clip, /* response */
        google::protobuf::NewCallback(&got_rpc_reply_callback, instance));

    // Wait until we get a reply from the server
    instance->rpc_condition_variable.wait(rpc_lock, [&]{return !instance->rpc_waiting_reply;});

    instance->clip = instance->rpc_clip.content();
    if (!instance->clip.empty()) {
        *out_content_size = instance->clip.size();
        // argh!
        *out_content = const_cast<void*>(static_cast<const void*>(instance->clip.data()));
    }
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
    auto instance = global_mir_instance();

    auto display = new MirDisplayInfo;
    mir_connection_get_display_info(instance->connection(), display);

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
    auto instance = global_mir_instance();

    // TODO<mir>: Careful with this cast
    return reinterpret_cast<EGLNativeDisplayType>(mir_connection_get_egl_native_display(instance->connection()));
}

UAUiWindowProperties* ua_ui_window_properties_new_for_normal_window()
{
    return (new uamc::WindowProperties)->as_u_window_properties();
}

void ua_ui_window_properties_destroy(UAUiWindowProperties* u_properties)
{
    auto properties = uamc::WindowProperties::from_u_window_properties(u_properties);
    delete properties;
}

void ua_ui_window_properties_set_titlen(UAUiWindowProperties* u_properties, const char* title, size_t title_length)
{
    auto properties = uamc::WindowProperties::from_u_window_properties(u_properties);

    properties->set_title(title, title_length);
}

const char* ua_ui_window_properties_get_title(UAUiWindowProperties* u_properties)
{
    auto properties = uamc::WindowProperties::from_u_window_properties(u_properties);
    return properties->surface_parameters().name;
}

void ua_ui_window_properties_set_role(UAUiWindowProperties* u_properties, UAUiWindowRole role)
{
    auto properties = uamc::WindowProperties::from_u_window_properties(u_properties);
    properties->set_role(role);
}

void ua_ui_window_properties_set_input_cb_and_ctx(UAUiWindowProperties* u_properties, UAUiWindowInputEventCb cb, void* ctx)
{
    auto properties = uamc::WindowProperties::from_u_window_properties(u_properties);
    properties->set_input_cb_and_ctx(cb, ctx);
}

void ua_ui_window_properties_set_event_cb_and_ctx(UAUiWindowProperties* u_properties, UAUiWindowEventCb cb, void* ctx)
{
    auto properties = uamc::WindowProperties::from_u_window_properties(u_properties);
    properties->set_event_cb_and_ctx(cb, ctx);
}

void ua_ui_window_properties_set_dimensions(UAUiWindowProperties *u_properties, uint32_t width, uint32_t height)
{
    auto properties = uamc::WindowProperties::from_u_window_properties(u_properties);
    properties->set_dimensions(width, height);
}

UAUiWindow* ua_ui_window_new_for_application_with_properties(UApplicationInstance* u_instance, UAUiWindowProperties* u_properties)
{
    auto instance = uamc::Instance::from_u_application_instance(u_instance);
    auto properties = uamc::WindowProperties::from_u_window_properties(u_properties);

    auto window = new uamc::Window(*instance, properties);
    return window->as_u_window();
}

void ua_ui_window_destroy(UAUiWindow* u_window)
{
    auto window = uamc::Window::from_u_window(u_window);
    delete window;
}

void ua_ui_window_get_size(UAUiWindow* u_window, uint32_t *width, uint32_t *height)
{
    auto window = uamc::Window::from_u_window(u_window);
    window->get_size(width, height);
}

UStatus ua_ui_window_move(UAUiWindow* u_window, uint32_t x, uint32_t y)
{
    // TODO<papi,mir>: Implement. Assuming this should exist on mirclient?
    (void) u_window;
    (void) x;
    (void) y;
    return (UStatus) 0;
}

UStatus ua_ui_window_resize(UAUiWindow* u_window, uint32_t width, uint32_t height)
{
    // TODO<mir>: Implement
    (void) u_window;
    (void) width;
    (void) height;
    return (UStatus) 0;
}

UStatus ua_ui_window_hide(UAUiWindow* u_window)
{
    auto window = uamc::Window::from_u_window(u_window);
    window->hide();
    return (UStatus) 0;
}

UStatus ua_ui_window_show(UAUiWindow* u_window)
{
    auto window = uamc::Window::from_u_window(u_window);
    window->show();
    return (UStatus) 0;
}

void ua_ui_window_request_fullscreen(UAUiWindow* u_window)
{
    auto window = uamc::Window::from_u_window(u_window);
    window->request_fullscreen();
}

EGLNativeWindowType ua_ui_window_get_native_type(UAUiWindow* u_window)
{
    auto window = uamc::Window::from_u_window(u_window);
    return window->get_native_type();
}

int ua_ui_window_is_focused(UAUiWindow *u_window)
{
    auto window = uamc::Window::from_u_window(u_window);
    return window->is_focused();
}

// TODO: Sensors
}
