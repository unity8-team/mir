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
 *              Ricardo Mendoza <ricardo.mendoza@canonical.com
 */

#include <ubuntu/application/ui/init.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/session_credentials.h>
#include <ubuntu/application/ui/setup.h>
#include <ubuntu/application/ui/surface.h>
#include <ubuntu/application/ui/surface_factory.h>
#include <ubuntu/application/ui/surface_properties.h>

#include <ubuntu/ui/session_service.h>

#include <utils/Log.h>

// C apis
#include <ubuntu/application/ui/ubuntu_application_ui.h>
#include <ubuntu/application/lifecycle_delegate.h>

// C-API implementation
namespace
{
struct _UApplicationLifecycleDelegate : public ubuntu::application::_UApplicationLifecycleDelegate
{
    _UApplicationLifecycleDelegate(void *context) :
                                    application_started_cb(NULL),
                                    application_about_to_stop_cb(NULL),
                                    context(context),
                                    refcount(1)
    {
    }

    void on_application_started()
    {
        if (!application_started_cb)
            return;

        application_started_cb(NULL, this->context);
    }

    void on_application_about_to_stop()
    {
        if (!application_about_to_stop_cb)
            return;

        application_about_to_stop_cb(NULL, this->context);
    }

    u_on_application_started application_started_cb;
    u_on_application_about_to_stop application_about_to_stop_cb;
    void *context;

    unsigned refcount;
};
}

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

void
ubuntu_application_ui_init(int argc, char**argv)
{
    ubuntu::application::ui::init(argc, argv);
}

::StageHint
ubuntu_application_ui_setup_get_stage_hint()
{
    return static_cast<StageHint>(
               ubuntu::application::ui::Setup::instance()->stage_hint());
}

::FormFactorHint
ubuntu_application_ui_setup_get_form_factor_hint()
{
    return static_cast<FormFactorHint>(
               ubuntu::application::ui::Setup::instance()->form_factor_hint());
}

void
ubuntu_application_ui_start_a_new_session(SessionCredentials* creds)
{
    if (session != NULL)
        return;

    ubuntu::application::ui::SessionCredentials sc(creds);
    session = ubuntu::ui::SessionService::instance()->start_a_new_session(sc);
}

void
ubuntu_application_ui_set_clipboard_content(void* data,
                                            size_t size)
{
    static const char mime_type[ubuntu::application::ui::Clipboard::Content::MAX_MIME_TYPE_SIZE] = "none/none";

    ubuntu::application::ui::Clipboard::Content content(mime_type, data, size);

    ubuntu::application::ui::Session::clipboard()->set_content(content);
}

void
ubuntu_application_ui_get_clipboard_content(void** data,
                                            size_t* size)
{
    ubuntu::application::ui::Clipboard::Content content(ubuntu::application::ui::Session::clipboard()->get_content());

    *data = content.data;
    *size = content.data_size;
}

void
ubuntu_application_ui_create_display_info(
    ubuntu_application_ui_physical_display_info* info,
    size_t index)
{
    *info = make_holder(
        ubuntu::application::ui::Session::physical_display_info(
            static_cast<ubuntu::application::ui::PhysicalDisplayIdentifier>(index)));
}

void
ubuntu_application_ui_destroy_display_info(
    ubuntu_application_ui_physical_display_info info)
{
    auto s = static_cast<Holder<ubuntu::application::ui::PhysicalDisplayInfo::Ptr>*>(info);
    delete s;
}

int32_t
ubuntu_application_ui_query_horizontal_resolution(
    ubuntu_application_ui_physical_display_info info)
{
    auto s = static_cast<Holder<ubuntu::application::ui::PhysicalDisplayInfo::Ptr>*>(info);
    return s->value->horizontal_resolution();
}

int32_t
ubuntu_application_ui_query_vertical_resolution(
    ubuntu_application_ui_physical_display_info info)
{
    auto s = static_cast<Holder<ubuntu::application::ui::PhysicalDisplayInfo::Ptr>*>(info);
    return s->value->vertical_resolution();
}

float
ubuntu_application_ui_query_horizontal_dpi(
    ubuntu_application_ui_physical_display_info info)
{
    auto s = static_cast<Holder<ubuntu::application::ui::PhysicalDisplayInfo::Ptr>*>(info);
    return s->value->horizontal_dpi();
}

float
ubuntu_application_ui_query_vertical_dpi(
    ubuntu_application_ui_physical_display_info info)
{
    auto s = static_cast<Holder<ubuntu::application::ui::PhysicalDisplayInfo::Ptr>*>(info);
    return s->value->vertical_dpi();
}

void
ubuntu_application_ui_create_surface(
    ubuntu_application_ui_surface* out_surface,
    const char* title,
    int width,
    int height,
    SurfaceRole role,
    uint32_t flags,
    input_event_cb cb,
    void* ctx)
{
    if (session == NULL)
    {
        // TODO: Report the error here.
        return;
    }
    ubuntu::application::ui::SurfaceProperties props =
    {
        "test",
        width,
        height,
        static_cast<ubuntu::application::ui::SurfaceRole>(role),
        flags
    };

    ubuntu::application::ui::Surface::Ptr surface =
        session->create_surface(
            props,
            ubuntu::application::ui::input::Listener::Ptr(
                new CallbackEventListener(cb, ctx)));

    *out_surface = make_holder(surface);
}

void
ubuntu_application_ui_request_fullscreen_for_surface(ubuntu_application_ui_surface surface)
{
    if (session == NULL)
    {
        // TODO: Report the error here.
        return;
    }

    auto s = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(surface);
    session->toggle_fullscreen_for_surface(s->value);
}

void
ubuntu_application_ui_destroy_surface(
    ubuntu_application_ui_surface surface)
{
    auto s = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(surface);
    delete s;
}

EGLNativeWindowType
ubuntu_application_ui_surface_to_native_window_type(
    ubuntu_application_ui_surface surface)
{
    auto s = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(surface);
    return s->value->to_native_window_type();
}

void 
ubuntu_application_ui_show_surface(
    ubuntu_application_ui_surface surface)
{
    if (session == NULL)
        return;

    auto s = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(surface);
    s->value->set_visible(session->get_session_pid(), true);
}

void 
ubuntu_application_ui_hide_surface(
    ubuntu_application_ui_surface surface)
{
    if (session == NULL)
        return;
  
    auto s = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(surface);
    s->value->set_visible(session->get_session_pid(), false);
}

void 
ubuntu_application_ui_move_surface_to(
    ubuntu_application_ui_surface surface,
    int x,
    int y)
{
    auto s = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(surface);
    s->value->move_to(x, y);
}

void 
ubuntu_application_ui_resize_surface_to(
    ubuntu_application_ui_surface surface,
    int w,
    int h)
{
    auto s = static_cast<Holder<ubuntu::application::ui::Surface::Ptr>*>(surface);
    s->value->resize(w, h);
}

/*
 * NEW LIFECYCLE DELEGATE CODE 
 */

UApplicationLifecycleDelegate
u_application_lifecycle_delegate_new()
{
    ALOGI("%s()", __PRETTY_FUNCTION__);

    ubuntu::application::_UApplicationLifecycleDelegate::Ptr p(new _UApplicationLifecycleDelegate(NULL));
    session->install_lifecycle_delegate(p);

    return p.get();
}

void
u_application_lifecycle_delegate_destroy(UApplicationLifecycleDelegate delegate)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);

    if (s->refcount)
        return;

    delete s;
}

void
u_application_lifecycle_delegate_ref(UApplicationLifecycleDelegate delegate)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);
    s->refcount++;
}

void
u_application_lifecycle_delegate_unref(UApplicationLifecycleDelegate delegate)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);
    if (s->refcount)
        s->refcount--;
}

void
u_application_lifecycle_delegate_set_application_started_cb(
    UApplicationLifecycleDelegate delegate,
    u_on_application_started cb)
{ 
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);
    s->application_started_cb = cb;
}

u_on_application_started
u_application_lifecycle_delegate_get_application_started_cb(
    UApplicationLifecycleDelegate delegate)
{ 
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);
    return s->application_started_cb;
}

void
u_application_lifecycle_delegate_set_application_about_to_stop_cb(
    UApplicationLifecycleDelegate delegate,
    u_on_application_about_to_stop cb)
{ 
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);
    s->application_about_to_stop_cb = cb;
}

u_on_application_about_to_stop
u_application_lifecycle_delegate_get_application_about_to_stop_cb(
    UApplicationLifecycleDelegate delegate)
{ 
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);
    return s->application_about_to_stop_cb;
}

void
u_application_lifecycle_delegate_set_context(
    UApplicationLifecycleDelegate delegate,
    void *context)
{
    ALOGI("%s():%d context=%p", __PRETTY_FUNCTION__, __LINE__, context);
    
    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);
    if (s->context == NULL)
        s->context = context;
}

void*
u_application_lifecycle_delegate_get_context(
    UApplicationLifecycleDelegate delegate,
    void *context)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<_UApplicationLifecycleDelegate*>(delegate);
    return s->context;
}
