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

#include "application_instance_mirserver_priv.h"
#include "window_properties_mirserver_priv.h"
#include "window_mirserver_priv.h"

#include "mircommon/application_id_mir_priv.h"
#include "mircommon/application_description_mir_priv.h"
#include "mircommon/application_options_mir_priv.h"
#include "mircommon/mir/geometry/rectangles.h"

// C APIs
#include <ubuntu/application/init.h>
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
#include <mir/graphics/display_buffer.h>
#include <mir/graphics/platform.h>
#include <mir/graphics/graphic_buffer_allocator.h>
#include <mir/graphics/internal_client.h>
#include <mir/input/input_platform.h>

#include <assert.h>

#include <memory>
#include <functional>

namespace uam = ubuntu::application::mir;
namespace uams = ubuntu::application::mir::server;

namespace
{

struct MirServerContext
{
    std::shared_ptr<mir::graphics::Display> display;
    std::shared_ptr<mir::graphics::GraphicBufferAllocator> buffer_allocator;
    std::shared_ptr<mir::scene::SurfaceCoordinator> surface_coordinator;
    std::shared_ptr<mir::scene::SessionListener> session_listener;
    std::shared_ptr<mir::input::receiver::InputPlatform> input_platform;
    std::shared_ptr<mir::graphics::InternalClient> egl_client;
};

MirServerContext *
global_mirserver_context()
{
    static MirServerContext context;
    return &context;
}
}

extern "C"
{
void u_application_init(void *args)
{
    auto context = global_mirserver_context();
    mir::DefaultServerConfiguration* config = reinterpret_cast<mir::DefaultServerConfiguration*>(args);

    context->surface_coordinator = config->the_surface_coordinator();
    context->buffer_allocator = config->the_buffer_allocator();
    context->display = config->the_display();
    context->session_listener = config->the_session_listener();
    context->input_platform = mir::input::receiver::InputPlatform::create();
    context->egl_client = config->the_graphics_platform()->create_internal_client();
}

void u_application_finish()
{
    auto context = global_mirserver_context();

    context->display.reset();
    context->surface_coordinator.reset();
    context->session_listener.reset();
    context->input_platform.reset();
    context->egl_client.reset();
}

UApplicationInstance* u_application_instance_new_from_description_with_options(UApplicationDescription* u_description, UApplicationOptions* u_options)
{
    auto surface_coordinator = global_mirserver_context()->surface_coordinator;
    assert(surface_coordinator);
    auto session_listener = global_mirserver_context()->session_listener;
    assert(session_listener);

    auto description = uam::Description::from_u_application_description(u_description);
    auto options = uam::Options::from_u_application_options(u_options);

    auto instance = new uams::Instance(surface_coordinator, session_listener, description, options);

    return instance->as_u_application_instance();
}

void
u_application_instance_ref(UApplicationInstance *u_instance)
{
    auto instance = uams::Instance::from_u_application_instance(u_instance);
    instance->ref();
}
    
void
u_application_instance_unref(UApplicationInstance *u_instance)
{
    auto instance = uams::Instance::from_u_application_instance(u_instance);
    instance->unref();
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
    // TODO<mir,papi>: Implement. Probably need more arguments?
    (void) content;
    (void) content_size;
}

void ua_ui_get_clipboard_content(void** out_content, size_t* out_content_size)
{
    // TODO<mir,papi>: Implement
    *out_content = NULL;
    *out_content_size = 0;
}

UAUiDisplay* ua_ui_display_new_with_index(size_t index)
{
    // TODO<mir>: Make use of index. This is kind of strangely done...
    return reinterpret_cast<UAUiDisplay*>(index);
}

void ua_ui_display_destroy(UAUiDisplay* display)
{
    // TODO: Implement. Or is this a noop for mirserver?
    (void) display;
}

uint32_t ua_ui_display_query_horizontal_res(UAUiDisplay* display)
{
    (void) display; // TODO<mir>: Multiple displays

    auto mir_display = global_mirserver_context()->display;
    assert(mir_display);

    /* TODO: Get proper configuration */
    mir::geometry::Rectangles view_area;
    mir_display->for_each_display_buffer([&view_area](mir::graphics::DisplayBuffer const& db)
    {
        view_area.add(db.view_area());
    });
    mir::geometry::Size const display_size{view_area.bounding_rectangle().size};
    
    return display_size.width.as_uint32_t();
}

uint32_t ua_ui_display_query_vertical_res(UAUiDisplay* display)
{
    (void) display; // TODO<mir>: Multiple displays

    auto mir_display = global_mirserver_context()->display;
    assert(mir_display);

    /* TODO: Get proper configuration */
    mir::geometry::Rectangles view_area;
    mir_display->for_each_display_buffer([&view_area](mir::graphics::DisplayBuffer const& db)
    {
        view_area.add(db.view_area());
    });
    mir::geometry::Size const display_size{view_area.bounding_rectangle().size};
    
    return display_size.height.as_uint32_t();
}

EGLNativeDisplayType ua_ui_display_get_native_type(UAUiDisplay* display)
{
    auto internal_client = global_mirserver_context()->egl_client;
    assert(internal_client);
    
    return internal_client->egl_native_display();
}

namespace
{
static MirPixelFormat choose_pixel_format(std::shared_ptr<mir::graphics::GraphicBufferAllocator> const& allocator)
{
    auto formats = allocator->supported_pixel_formats();
    return formats[0];
}
}

UAUiWindowProperties* ua_ui_window_properties_new_for_normal_window()
{
    auto properties = new uams::WindowProperties();

    return properties->as_u_window_properties();
}

void ua_ui_window_properties_destroy(UAUiWindowProperties* u_properties)
{
    auto properties = uams::WindowProperties::from_u_window_properties(u_properties);
    delete properties;
}

void ua_ui_window_properties_set_titlen(UAUiWindowProperties* u_properties, const char* title, size_t title_length)
{
    auto properties = uams::WindowProperties::from_u_window_properties(u_properties);
    properties->set_title(title, title_length);
}

const char* ua_ui_window_properties_get_title(UAUiWindowProperties* u_properties)
{
    auto properties = uams::WindowProperties::from_u_window_properties(u_properties);
    return properties->surface_parameters().name.c_str();
}

void ua_ui_window_properties_set_role(UAUiWindowProperties* properties, UAUiWindowRole role)
{
    // TODO<papi>: Implement. Or is this a noop for us?
    (void) properties;
    (void) role;
}

void ua_ui_window_properties_set_event_cb_and_ctx(UAUiWindowProperties*, UAUiWindowEventCb, void*)
{
}

void ua_ui_window_properties_set_input_cb_and_ctx(UAUiWindowProperties* u_properties, UAUiWindowInputEventCb cb, void* ctx)
{
    auto properties = uams::WindowProperties::from_u_window_properties(u_properties);
    properties->set_input_cb_and_ctx(cb, ctx);
}

void ua_ui_window_properties_set_dimensions(UAUiWindowProperties *u_properties, uint32_t width, uint32_t height)
{
    auto properties = uams::WindowProperties::from_u_window_properties(u_properties);
    properties->set_dimensions(width, height);
}

UAUiWindow* ua_ui_window_new_for_application_with_properties(UApplicationInstance* u_instance, UAUiWindowProperties* u_properties)
{
    auto input_platform = global_mirserver_context()->input_platform;
    assert(input_platform);
    auto internal_client = global_mirserver_context()->egl_client;
    assert(internal_client);

    auto instance = uams::Instance::from_u_application_instance(u_instance);
    auto properties = uams::WindowProperties::from_u_window_properties(u_properties);
    
    // A bit of a wart.
    properties->set_pixel_format(choose_pixel_format(global_mirserver_context()->buffer_allocator));
    
    auto window = new uams::Window(*instance, properties, input_platform, internal_client);

    return window->as_u_window();
}

void ua_ui_window_destroy(UAUiWindow* u_window)
{
    auto window = uams::Window::from_u_window(u_window);
    delete window;
}

void ua_ui_window_get_size(UAUiWindow* u_window, uint32_t *width, uint32_t *height)
{
    (void) u_window;
    (void) width;
    (void) height;
}

UStatus ua_ui_window_move(UAUiWindow* window, uint32_t x, uint32_t y)
{
    // TODO<mir,papi>: Implement. But should this exist?
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

EGLNativeWindowType ua_ui_window_get_native_type(UAUiWindow* u_window)
{
    auto window = uams::Window::from_u_window(u_window);

    return window->get_native_type();
}

// TODO: Sensors
}
