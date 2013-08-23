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

#ifndef UBUNTU_UI_SESSION_SERVICE_C_API_H_
#define UBUNTU_UI_SESSION_SERVICE_C_API_H_

#include <ubuntu/visibility.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum
    {
        UNKNOWN_APP = -1,
        CAMERA_APP = 0,
        GALLERY_APP = 1,
        BROWSER_APP = 2,
        SHARE_APP = 3,
        PHONE_APP = 4,
        DIALER_APP = 5,
        MESSAGING_APP = 6,
        ADDRESSBOOK_APP = 7
    } ubuntu_ui_well_known_application;

    typedef const void* ubuntu_ui_session_properties;
    typedef const void* ubuntu_ui_session_preview_provider;

    typedef void (*ubuntu_ui_session_service_snapshot_cb)(const void* pixels, unsigned int width, unsigned int height, unsigned int x, unsigned int y, unsigned int source_width, unsigned int source_height, unsigned int stride, void* context);

    typedef void (*session_requested_cb)(ubuntu_ui_well_known_application app, void* context);
    typedef void (*session_born_cb)(ubuntu_ui_session_properties props, void* context);
    typedef void (*session_unfocused_cb)(ubuntu_ui_session_properties props, void* context);
    typedef void (*session_focused_cb)(ubuntu_ui_session_properties props, void* context);
    typedef void (*keyboard_geometry_changed_cb)(int x, int y, int width, int height, void* context);
    typedef void (*session_requested_fullscreen_cb)(ubuntu_ui_session_properties props, void* context);
    typedef void (*session_died_cb)(ubuntu_ui_session_properties props, void * context);

    typedef struct
    {
        session_requested_cb on_session_requested;
        session_born_cb on_session_born;
        session_unfocused_cb on_session_unfocused;
        session_focused_cb on_session_focused;
        keyboard_geometry_changed_cb on_keyboard_geometry_changed;
        session_requested_fullscreen_cb on_session_requested_fullscreen;
        session_died_cb on_session_died;

        void* context;
    } ubuntu_ui_session_lifecycle_observer;

    typedef void (*continue_task_cb)(int pid, void* context);
    typedef void (*suspend_task_cb)(int pid, void* context);

    typedef struct
    {
        continue_task_cb continue_task;
        suspend_task_cb suspend_task;

        void *context;
    } ubuntu_ui_task_controller;

    UBUNTU_DLL_PUBLIC void
    ubuntu_ui_install_task_controller(
        ubuntu_ui_task_controller *controller);

    UBUNTU_DLL_PUBLIC const char* 
    ubuntu_ui_session_properties_get_value_for_key(
        ubuntu_ui_session_properties props, 
        const char* key);

    UBUNTU_DLL_PUBLIC int 
    ubuntu_ui_session_properties_get_application_stage_hint(
        ubuntu_ui_session_properties props);

    UBUNTU_DLL_PUBLIC int 
    ubuntu_ui_session_properties_get_application_instance_id(
        ubuntu_ui_session_properties props);

    UBUNTU_DLL_PUBLIC const char* 
    ubuntu_ui_session_properties_get_desktop_file_hint(
        ubuntu_ui_session_properties props);

    UBUNTU_DLL_PUBLIC void 
    ubuntu_ui_session_install_session_lifecycle_observer(
        ubuntu_ui_session_lifecycle_observer* observer);

    UBUNTU_DLL_PUBLIC void 
    ubuntu_ui_session_unfocus_running_sessions();

    UBUNTU_DLL_PUBLIC void 
    ubuntu_ui_session_focus_running_session_with_id(int id);

    UBUNTU_DLL_PUBLIC void 
    ubuntu_ui_session_snapshot_running_session_with_id(int id, ubuntu_ui_session_service_snapshot_cb cb, void* context);

    UBUNTU_DLL_PUBLIC void 
    ubuntu_ui_session_trigger_switch_to_well_known_application(ubuntu_ui_well_known_application app);

    UBUNTU_DLL_PUBLIC int32_t
    ubuntu_ui_set_surface_trap(int x, int y, int width, int height);

    UBUNTU_DLL_PUBLIC void
    ubuntu_ui_unset_surface_trap(int32_t handle);

    UBUNTU_DLL_PUBLIC void
    ubuntu_ui_report_osk_visible(int x, int y, int width, int height);
    
    UBUNTU_DLL_PUBLIC void
    ubuntu_ui_report_osk_invisible();

    UBUNTU_DLL_PUBLIC void
    ubuntu_ui_report_notification_visible();
    
    UBUNTU_DLL_PUBLIC void
    ubuntu_ui_report_notification_invisible();

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_UI_SESSION_SERVICE_C_API_H_
