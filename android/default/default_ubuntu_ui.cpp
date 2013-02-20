/*
 * Copyright © 2012 Canonical Ltd.
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
 */

#include <ubuntu/ui/ubuntu_ui_session_service.h>

#include <ubuntu/ui/session_service.h>
#include <ubuntu/ui/session_enumerator.h>

#include <stdio.h>

#include <utils/Log.h>

namespace
{
struct SessionLifeCycleObserver : public ubuntu::ui::SessionLifeCycleObserver
{
    SessionLifeCycleObserver(ubuntu_ui_session_lifecycle_observer* observer) : observer(observer)
    {
    }

    void on_session_requested(ubuntu::ui::WellKnownApplication app)
    {
        if (!observer)
            return;

        if (!observer->on_session_requested)
            return;

        observer->on_session_requested(static_cast<ubuntu_ui_well_known_application>(app), observer->context);
    }

    void on_session_born(const ubuntu::ui::SessionProperties::Ptr& props)
    {
        if (!observer)
            return;

        if (!observer->on_session_born)
            return;

        observer->on_session_born(&props, observer->context);
    }

    void on_session_unfocused(const ubuntu::ui::SessionProperties::Ptr& props)
    {
        if (!observer)
            return;

        if (!observer->on_session_unfocused)
            return;

        observer->on_session_unfocused(&props, observer->context);
    }

    void on_session_focused(const ubuntu::ui::SessionProperties::Ptr& props)
    {
        if (!observer)
            return;

        if (!observer->on_session_focused)
            return;

        observer->on_session_focused(&props, observer->context);
    }

    void on_keyboard_geometry_changed(int x, int y, int width, int height)
    {
        if (!observer)
            return;

        if (!observer->on_keyboard_geometry_changed)
            return;

        observer->on_keyboard_geometry_changed(x, y, width, height, observer->context);
    }

    void on_session_requested_fullscreen(const ubuntu::ui::SessionProperties::Ptr& props)
    {
        if (!observer)
            return;

        if(!observer->on_session_requested_fullscreen)
            return;

        observer->on_session_requested_fullscreen(&props, observer->context);
    }

    void on_session_died(const ubuntu::ui::SessionProperties::Ptr& props)
    {
        if (!observer)
            return;

        if (!observer->on_session_died)
            return;

        observer->on_session_died(&props, observer->context);
    }

    ubuntu_ui_session_lifecycle_observer* observer;
};

}

const char* ubuntu_ui_session_properties_get_value_for_key(ubuntu_ui_session_properties props, const char* key)
{
    if (!props)
        return NULL;

    if (!key)
        return NULL;

    const ubuntu::ui::SessionProperties::Ptr* p = static_cast<const ubuntu::ui::SessionProperties::Ptr*>(props);

    return (*p)->value_for_key(key);
}

int ubuntu_ui_session_properties_get_application_stage_hint(ubuntu_ui_session_properties props)
{
    if (!props)
        return -1;

    const ubuntu::ui::SessionProperties::Ptr* p = static_cast<const ubuntu::ui::SessionProperties::Ptr*>(props);

    return (*p)->application_stage_hint();
}

int ubuntu_ui_session_properties_get_application_instance_id(ubuntu_ui_session_properties props)
{
    if (!props)
        return -1;

    const ubuntu::ui::SessionProperties::Ptr* p = static_cast<const ubuntu::ui::SessionProperties::Ptr*>(props);

    return (*p)->application_instance_id();
}

const char* ubuntu_ui_session_properties_get_desktop_file_hint(ubuntu_ui_session_properties props)
{
    if (!props)
        return NULL;

    const ubuntu::ui::SessionProperties::Ptr* p = static_cast<const ubuntu::ui::SessionProperties::Ptr*>(props);

    return (*p)->desktop_file_hint();
}

bool ubuntu_ui_session_preview_provider_update_session_preview_texture(ubuntu_ui_session_preview_provider pp, int id, GLuint texture, unsigned int* width, unsigned int* height)
{
    if (!pp)
        return false;

    const ubuntu::ui::SessionPreviewProvider::Ptr* spp =
        static_cast<const ubuntu::ui::SessionPreviewProvider::Ptr*>(pp);

    return (*spp)->get_or_update_session_preview(texture, *width, *height);
}

void ubuntu_ui_session_install_session_lifecycle_observer(ubuntu_ui_session_lifecycle_observer* observer)
{
    ubuntu::ui::SessionLifeCycleObserver::Ptr p(new SessionLifeCycleObserver(observer));
    ubuntu::ui::SessionService::instance()->install_session_lifecycle_observer(p);
}

void ubuntu_ui_session_unfocus_running_sessions()
{
    ubuntu::ui::SessionService::instance()->unfocus_running_sessions();
}

void ubuntu_ui_session_focus_running_session_with_id(int id)
{
    ubuntu::ui::SessionService::instance()->focus_running_session_with_id(id);
}

void ubuntu_ui_session_snapshot_running_session_with_id(int id, ubuntu_ui_session_service_snapshot_cb cb, void* context)
{
    ubuntu::ui::SessionSnapshot::Ptr ss = ubuntu::ui::SessionService::instance()->snapshot_running_session_with_id(id);

    if (cb)
    {
        ALOGI("screenshot buffer (%d, %d) geometry (%d, %d, %d, %d)\n", ss->width(), ss->height(), ss->x(), ss->y(), ss->source_width(), ss->source_height());
        cb(ss->pixel_data(), ss->width(), ss->height(), ss->x(), ss->y(),
           ss->source_width(), ss->source_height(), ss->stride(), context);
    }
}

void ubuntu_ui_session_trigger_switch_to_well_known_application(ubuntu_ui_well_known_application app)
{
    ubuntu::ui::SessionService::instance()->trigger_switch_to_well_known_application(
        static_cast<ubuntu::ui::WellKnownApplication>(app));
}

int32_t ubuntu_ui_set_surface_trap(int32_t x, int32_t y, int32_t width, int32_t height)
{
    return ubuntu::ui::SessionService::instance()->set_surface_trap(x, y, width, height);
}

void ubuntu_ui_unset_surface_trap(int32_t handle)
{
    ubuntu::ui::SessionService::instance()->unset_surface_trap(handle);
}

void ubuntu_ui_report_osk_visible(int x, int y, int width, int height)
{
    ubuntu::ui::SessionService::instance()->report_osk_visible(x, y, width, height);
}

void ubuntu_ui_report_osk_invisible()
{
    ubuntu::ui::SessionService::instance()->report_osk_invisible();
}

void ubuntu_ui_report_notification_visible()
{
    ubuntu::ui::SessionService::instance()->report_notification_visible();
}

void ubuntu_ui_report_notification_invisible()
{
    ubuntu::ui::SessionService::instance()->report_notification_invisible();
}
