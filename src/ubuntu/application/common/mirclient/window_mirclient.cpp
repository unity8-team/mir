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

#include "window_mirclient_priv.h"
#include "application_instance_mirclient_priv.h"
#include "window_properties_mirclient_priv.h"
#include "window_mirclient_priv.h"

#include "mircommon/event_helpers_mir.h"

namespace uamc = ubuntu::application::mir::client;
namespace uaum = ubuntu::application::ui::mir;

namespace ubuntu
{
namespace application
{
namespace mir
{
namespace client
{

static void
ua_ui_window_mir_handle_event(MirSurface* surface, MirEvent const* mir_ev, void* ctx)
{
    // TODO<mir>: Perhaps this indicates that mirclient library should not pass a surface argument here.
    (void) surface;

    WindowEvent ubuntu_ev;

    bool translated_event = uaum::event_to_ubuntu_event(mir_ev, ubuntu_ev);

    // Mir sends some events which platform API does not represent as platform-api events.
    if (!translated_event)
        return;

    auto window = static_cast<uamc::Window*>(ctx);
    window->process_event(ubuntu_ev);

    UAUiWindowEventCb user_callback = window->get_user_callback();
    if (user_callback != nullptr) {
        user_callback(window->get_user_callback_context(), &ubuntu_ev);
    } else {
        // Fallback to the old, deprecated, API for handling events
        Event ubuntu_deprecated_ev;

        bool ok = uaum::event_to_ubuntu_deprecated_event(mir_ev, ubuntu_deprecated_ev);

        if (ok) {
            UAUiWindowInputEventCb user_input_callback = window->get_user_input_callback();
            user_input_callback(window->get_user_callback_context(), &ubuntu_deprecated_ev);
        }
    }

}

}
}
}
}

namespace
{

static MirPixelFormat
mir_choose_default_pixel_format(MirConnection *connection)
{
    MirDisplayInfo info;
    mir_connection_get_display_info(connection, &info);
    return info.supported_pixel_format[0];
}

static MirSurface*
create_surface_with_parameters(MirConnection *connection,
                               MirSurfaceParameters parameters)
{
}

}

uamc::Window::Window(uamc::Instance& instance,
                     uamc::WindowProperties* properties)
    : instance(instance),
      focused(0)
{
    user_event_callback = properties->event_cb();
    user_event_callback_context = properties->event_cb_context();
    user_input_callback = properties->input_cb();

    window_properties = WindowPropertiesPtr(properties,
        [](uamc::WindowProperties *p)
        {
            ua_ui_window_properties_destroy(p->as_u_window_properties());
        });

    MirSurfaceParameters parameters = properties->surface_parameters();
    auto connection = instance.connection();
    parameters.pixel_format = mir_choose_default_pixel_format(connection);

    auto mir_surface = mir_connection_create_surface_sync(connection, &parameters);
    // TODO: create_surface_sync is unsafe as there is a race between setting the event handler and receiving surfaces

    if (properties->surface_type()) {
        // TODO: Should I bother checking the result?
        mir_surface_set_type(mir_surface, properties->surface_type());
    }
    
    MirEventDelegate delegate = 
        { 
            uamc::ua_ui_window_mir_handle_event,
            this
        };
    mir_surface_set_event_handler(mir_surface, &delegate);
    surface = SurfacePtr(mir_surface, 
        [](MirSurface *s)
        {
            mir_surface_release_sync(s);
        });

    // FIXME: this only really applicable for phone & tablet case
    state_before_hiding = U_MAXIMIZED_STATE;
}

UAUiWindow* uamc::Window::as_u_window()
{
    return static_cast<UAUiWindow*>(this);
}

uamc::Window* uamc::Window::from_u_window(UAUiWindow *u_window)
{
    return static_cast<uamc::Window*>(u_window);
}

EGLNativeWindowType uamc::Window::get_native_type()
{
    return reinterpret_cast<EGLNativeWindowType>(mir_surface_get_egl_native_window(surface.get()));
}

UApplicationUiWindowState uamc::Window::state() const
{
    return static_cast<UApplicationUiWindowState>(mir_surface_get_state(surface.get()));
}

void uamc::Window::set_state(const UApplicationUiWindowState state)
{
    mir_surface_set_state(surface.get(), static_cast<MirSurfaceState>(state));
}

void uamc::Window::get_size(uint32_t *width, uint32_t *height)
{
    MirSurfaceParameters parameters;
    mir_surface_get_parameters(surface.get(), &parameters);
    *width = parameters.width;
    *height = parameters.height;
}

void uamc::Window::hide()
{
    state_before_hiding = state();
    set_state(U_MINIMIZED_STATE);
}

void uamc::Window::show()
{
    if (state() == U_MINIMIZED_STATE) {
        set_state(state_before_hiding);
    }
}

void uamc::Window::request_fullscreen()
{
    set_state(U_FULLSCREEN_STATE);
}

void uamc::Window::process_event(const WindowEvent &ev)
{
    if (ev.type == SURFACE_WEVENT_TYPE && ev.surface.attribute == SURFACE_ATTRIBUTE_FOCUS) {
        focused = ev.surface.value;
    }
}
