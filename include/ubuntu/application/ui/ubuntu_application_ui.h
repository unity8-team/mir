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
#ifndef UBUNTU_APPLICATION_UI_H_
#define UBUNTU_APPLICATION_UI_H_

#include "ubuntu/application/ui/input/event.h"

#include <EGL/egl.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum
    {
        USER_SESSION_TYPE = 0,
        SYSTEM_SESSION_TYPE = 1
    } SessionType;

    typedef enum
    {
        APPLICATION_SUPPORTS_OVERLAYED_MENUBAR = 0,
        APPLICATION_DOES_NOT_SUPPORT_OVERLAYED_MENUBAR = 1
    } MenuBarSupport;

    typedef enum
    {
        MAIN_STAGE_HINT = 0,
        INTEGRATION_STAGE_HINT = 1,
        SHARE_STAGE_HINT = 2,
        CONTENT_PICKING_STAGE_HINT = 3,
        SIDE_STAGE_HINT = 4,
        CONFIGURATION_STAGE_HINT = 5
    }
    StageHint;

    typedef enum
    {
        DESKTOP_FORM_FACTOR_HINT = 0,
        PHONE_FORM_FACTOR_HINT = 1,
        TABLET_FORM_FACTOR_HINT = 2
    } FormFactorHint;

    typedef enum
    {
        DASH_ACTOR_ROLE = 0,
        MAIN_ACTOR_ROLE = 1,
        INDICATOR_ACTOR_ROLE = 2,
        NOTIFICATIONS_ACTOR_ROLE = 3,
        GREETER_ACTOR_ROLE = 4,
        LAUNCHER_ACTOR_ROLE = 5,
        ON_SCREEN_KEYBOARD_ACTOR_ROLE = 6,
        SHUTDOWN_DIALOG_ACTOR_ROLE = 7
    } SurfaceRole;

    typedef enum
    {
        IS_OPAQUE_FLAG = 1
    } SurfaceFlags;

    enum
    {
        MAX_APPLICATION_NAME_LENGTH = 512
    };

    typedef struct
    {
        typedef void (*on_application_resumed)(void* ctx);
        typedef void (*on_application_suspended)(void* ctx);

        typedef void (*on_application_focused)(void* ctx);
        typedef void (*on_application_unfocused)(void* ctx);

        // Application-specific settings
        SessionType session_type;
        MenuBarSupport menu_bar_support;
        char application_name[MAX_APPLICATION_NAME_LENGTH];

        // Lifecycle callbacks
        on_application_resumed on_application_resumed_cb;
        on_application_suspended on_application_suspended_cb;
        on_application_focused on_application_focused_cb;
        on_application_unfocused on_application_unfocused_cb;

        void* context;
    } SessionCredentials;

    typedef void (*input_event_cb)(void* ctx, const Event* ev);

    typedef void* ubuntu_application_ui_physical_display_info;
    typedef void* ubuntu_application_ui_session;
    typedef void* ubuntu_application_ui_surface;

    void
    ubuntu_application_ui_init(
        int argc,
        char**argv);

    StageHint
    ubuntu_application_ui_setup_get_stage_hint();

    FormFactorHint
    ubuntu_application_ui_setup_get_form_factor_hint();

    void
    ubuntu_application_ui_start_a_new_session(SessionCredentials* creds);

    void
    ubuntu_application_ui_set_clipboard_content(void* data,
                                                size_t size); 

    void
    ubuntu_application_ui_get_clipboard_content(void** data,
                                                size_t* size); 

    void
    ubuntu_application_ui_create_display_info(
        ubuntu_application_ui_physical_display_info* info,
        size_t index);

    void
    ubuntu_application_ui_destroy_display_info(
        ubuntu_application_ui_physical_display_info info);

    int32_t
    ubuntu_application_ui_query_horizontal_resolution(
        ubuntu_application_ui_physical_display_info info);

    int32_t
    ubuntu_application_ui_query_vertical_resolution(
        ubuntu_application_ui_physical_display_info info);

    float
    ubuntu_application_ui_query_horizontal_dpi(
        ubuntu_application_ui_physical_display_info info);

    float
    ubuntu_application_ui_query_vertical_dpi(
        ubuntu_application_ui_physical_display_info info);

    void
    ubuntu_application_ui_create_surface(
        ubuntu_application_ui_surface* out_surface,
        const char* title,
        int width,
        int height,
        SurfaceRole role,
        uint32_t flags,
        input_event_cb cb,
        void* ctx);

    void
    ubuntu_application_ui_request_fullscreen_for_surface(
        ubuntu_application_ui_surface surface);

    void
    ubuntu_application_ui_destroy_surface(
        ubuntu_application_ui_surface surface);

    EGLNativeWindowType
    ubuntu_application_ui_surface_to_native_window_type(
        ubuntu_application_ui_surface surface);

    void
    ubuntu_application_ui_show_surface(
        ubuntu_application_ui_surface surface);

    void
    ubuntu_application_ui_hide_surface(
        ubuntu_application_ui_surface surface);

    void
    ubuntu_application_ui_move_surface_to(
        ubuntu_application_ui_surface surface,
        int x,
        int y);

    void
    ubuntu_application_ui_resize_surface_to(
        ubuntu_application_ui_surface surface,
        int w,
        int h);

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_UI_H_
