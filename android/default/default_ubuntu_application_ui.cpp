/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 *              Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

// Private
#include <private/application/ui/init.h>
#include <private/application/ui/session.h>
#include <private/application/ui/session_credentials.h>
#include <private/application/ui/setup.h>
#include <private/application/ui/surface.h>
#include <private/application/ui/surface_factory.h>
#include <private/application/ui/surface_properties.h>

// Public C apis
#include <private/application/ui/ubuntu_application_ui.h>
#include <ubuntu/application/instance.h>
#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/ui/options.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/window.h>
#include <ubuntu/application/ui/clipboard.h>
#include <ubuntu/application/ui/display.h>

// ver2.0 Private
#include <private/application/ui/window_internal.h>

#include <private/ui/session_service.h>
#include <private/application/ui/ubuntu_application_ui.h>
#include <private/application/application.h>

#include <utils/Log.h>

namespace
{
ubuntu::application::ui::Session::Ptr session;

struct CallbackEventListener : public ubuntu::application::ui::input::Listener
{
    CallbackEventListener(input_event_cb cb, void* context) : cb(cb),
        context(context)
    {
    }

    void on_new_event(const ::Event& e)
    {
        if (cb)
            cb(context, &e);
    }

    input_event_cb cb;
    void* context;
};

template<typename T>
struct Holder
{
    Holder(const T&value = T()) : value(value)
    {
    }

    T value;
};

template<typename T>
Holder<T>* make_holder(const T& value)
{
    return new Holder<T>(value);
}
}

/*
 * Clipboard
 */

void
ua_ui_set_clipboard_content(void* data,
                                            size_t size)
{
    static const char mime_type[ubuntu::application::ui::Clipboard::Content::MAX_MIME_TYPE_SIZE] = "none/none";

    ubuntu::application::ui::Clipboard::Content content(mime_type, data, size);

    ubuntu::application::ui::Session::clipboard()->set_content(content);
}

void
ua_ui_get_clipboard_content(void** data,
                                            size_t* size)
{
    ubuntu::application::ui::Clipboard::Content content(ubuntu::application::ui::Session::clipboard()->get_content());

    *data = content.data;
    *size = content.data_size;
}

/*
 * Display
 */

UAUiDisplay*
ua_ui_display_new_with_index(
    size_t index)
{
    return make_holder(
        ubuntu::application::ui::Session::physical_display_info(
            static_cast<ubuntu::application::ui::PhysicalDisplayIdentifier>(index)));
}

void
ua_ui_display_destroy(
    UAUiDisplay* display)
{
    auto s = static_cast<Holder<ubuntu::application::ui::PhysicalDisplayInfo::Ptr>*>(display);
    delete s;
}

uint32_t
ua_ui_display_query_horizontal_res(
    UAUiDisplay* display)
{
    auto s = static_cast<Holder<ubuntu::application::ui::PhysicalDisplayInfo::Ptr>*>(display);
    return s->value->horizontal_resolution();
}

uint32_t
ua_ui_display_query_vertical_res(
    UAUiDisplay* display)
{
    auto s = static_cast<Holder<ubuntu::application::ui::PhysicalDisplayInfo::Ptr>*>(display);
    return s->value->vertical_resolution();
}

EGLNativeDisplayType
ua_ui_display_get_native_type(
    UAUiDisplay* display)
{
    // Always EGL_DEFAULT_DISPLAY with android EGL.
    return EGL_DEFAULT_DISPLAY;
}                              

/*
 * Window Properties
 */

UAUiWindowProperties*
ua_ui_window_properties_new_for_normal_window()
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    ubuntu::application::ui::WindowProperties::Ptr p(
        new ubuntu::application::ui::WindowProperties()
        );

    return make_holder(p);
}

void
ua_ui_window_properties_destroy(
    UAUiWindowProperties *properties)
{
    auto p = static_cast<Holder<ubuntu::application::ui::WindowProperties::Ptr>*>(properties);
    
    if (p)
        delete p;
}

void
ua_ui_window_properties_set_titlen(
    UAUiWindowProperties *properties,
    const char *title,
    size_t size)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    auto p = static_cast<Holder<ubuntu::application::ui::WindowProperties::Ptr>*>(properties);
    p->value->set_titlen(title, size);
}

const char*
ua_ui_window_properties_get_title(
    UAUiWindowProperties *properties)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    auto p = static_cast<Holder<ubuntu::application::ui::WindowProperties::Ptr>*>(properties);
    return p->value->get_title();
}

void
ua_ui_window_properties_set_role(
    UAUiWindowProperties *properties,
    UAUiWindowRole role)
{
    ALOGI("%s():%d %p %d", __PRETTY_FUNCTION__, __LINE__, properties, role);
    auto p = static_cast<Holder<ubuntu::application::ui::WindowProperties::Ptr>*>(properties);
    p->value->set_role(role);
}

UAUiWindowRole
ua_ui_window_properties_get_role(
    UAUiWindowProperties *properties)
{
    auto p = static_cast<Holder<ubuntu::application::ui::WindowProperties::Ptr>*>(properties);
    return p->value->get_role();
}

void
ua_ui_window_properties_set_input_cb_and_ctx(
    UAUiWindowProperties *properties,
    UAUiWindowInputEventCb cb,
    void *ctx)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    auto p = static_cast<Holder<ubuntu::application::ui::WindowProperties::Ptr>*>(properties);
    p->value->set_input_event_cb_and_ctx(cb, ctx);
}

void
ua_ui_window_properties_set_event_cb_and_ctx(UAUiWindowProperties*, UAUiWindowEventCb, void *)
{
}

void
ua_ui_window_properties_set_dimensions(
    UAUiWindowProperties *properties,
    uint32_t width,
    uint32_t height)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    (void) width;
    (void) height;
}

/*
 * Session
 */

UAUiSessionProperties*
ua_ui_session_properties_new()
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    ubuntu::application::ui::SessionProperties::Ptr props(
        new ubuntu::application::ui::SessionProperties()
        );

    return make_holder(props);
}

void
ua_ui_session_properties_set_type(
    UAUiSessionProperties* properties,
    UAUiSessionType type)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    auto p = static_cast<Holder<ubuntu::application::ui::SessionProperties::Ptr>*>(properties);

    if (p)
        p->value->set_type(static_cast<ubuntu::application::ui::SessionType>(type));
}

void
ua_ui_session_properties_set_remote_pid(
    UAUiSessionProperties *properties,
    uint32_t pid)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    auto p = static_cast<Holder<ubuntu::application::ui::SessionProperties::Ptr>*>(properties);
    p->value->set_remote_pid(pid);
}

UAUiSession*
ua_ui_session_new_with_properties(
    UAUiSessionProperties *properties)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    if (session != NULL)
        return session.get();
    
    auto p = static_cast<Holder<ubuntu::application::ui::SessionProperties::Ptr>*>(properties);

    SessionCredentials creds = {
            static_cast<SessionType>(p->value->get_type()),
            APPLICATION_SUPPORTS_OVERLAYED_MENUBAR,
            "QtUbuntu",
            p->value->get_remote_pid(),
            NULL
    };    

    ubuntu::application::ui::SessionCredentials sc(&creds);
    session = ubuntu::ui::SessionService::instance()->start_a_new_session(sc);

    return session.get();
}

/*
 * Window (Surface)
 */

UAUiWindow*
ua_ui_window_new_for_application_with_properties(
    UApplicationInstance *instance,
    UAUiWindowProperties *properties)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    if (session == NULL)
        return NULL;
    
    auto p = static_cast<Holder<ubuntu::application::ui::WindowProperties::Ptr>*>(properties);

    ubuntu::application::ui::SurfaceProperties props =
    {
        "test",
        0,
        0,
        static_cast<ubuntu::application::ui::SurfaceRole>(p->value->get_role()),
        0, //FIXME: Set flags
        true
    };

    ubuntu::application::ui::Surface::Ptr surface =
        session->create_surface(
            props,
            ubuntu::application::ui::input::Listener::Ptr(
                new CallbackEventListener(p->value->get_input_cb(),
                                          p->value->get_ctx())));

    auto inst = static_cast<Holder<ubuntu::application::Instance::Ptr>*>(instance);
    auto desc = static_cast<Holder<ubuntu::application::Description::Ptr>*>(inst->value->get_description());
    auto dele = static_cast<Holder<ubuntu::application::LifecycleDelegate::Ptr>*>(desc->value->get_lifecycle_delegate());

    session->install_lifecycle_delegate(dele->value);

    return make_holder(surface);
}

void
ua_ui_window_destroy(
    UAUiWindow *window)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    //auto p = static_cast<Holder<ubuntu::application::ui::Window::Ptr*>*>(window);
    auto p = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(window);

    if (p)
        delete p;
}

UAUiWindowId
ua_ui_window_get_id(
    UAUiWindow *window)
{
    //auto p = static_cast<Holder<ubuntu::application::ui::Window::Ptr*>*>(window);
    auto p = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(window);
    
    return p->value->get_id();
}

UStatus
ua_ui_window_move(
    UAUiWindow *window,
    uint32_t new_x,
    uint32_t new_y)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    auto p = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(window);
    p->value->move_to(new_x, new_y);
    
    return U_STATUS_SUCCESS;
}

UStatus
ua_ui_window_resize(
    UAUiWindow *window,
    uint32_t new_width,
    uint32_t new_height)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    //auto p = static_cast<Holder<ubuntu::application::ui::Window::Ptr*>*>(window);
    auto p = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(window);
    p->value->resize(new_width, new_height);

    return U_STATUS_SUCCESS;
}

UStatus
ua_ui_window_hide(
    UAUiWindow *window)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    //auto p = static_cast<Holder<ubuntu::application::ui::Window::Ptr*>*>(window);
    auto p = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(window);
    p->value->set_visible(session->get_session_pid(), false);

    return U_STATUS_SUCCESS;
}

UStatus
ua_ui_window_show(
    UAUiWindow *window)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    auto p = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(window);
    p->value->set_visible(session->get_session_pid(), true);

    return U_STATUS_SUCCESS;
}

void
ua_ui_window_request_fullscreen(
    UAUiWindow *window)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    if (session == NULL)
        return;

    auto p = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(window);
    session->toggle_fullscreen_for_surface(p->value);
}

EGLNativeWindowType
ua_ui_window_get_native_type(
    UAUiWindow *window)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    auto p = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(window);
    return p->value->to_native_window_type();
}
