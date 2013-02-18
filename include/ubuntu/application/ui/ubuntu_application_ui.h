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

    /** \defgroup require_session_functions Functions that require an active session registered with the application manager/service providers.
     * \defgroup ui_access Functions that provide access to UI-specific functionality.
     */

    /**
     * Specifies the session type.
     * \ingroup ui_access
     * \attention System sessions require special privileges to be registered with the service providers.
     */
    typedef enum
    {
        USER_SESSION_TYPE = 0, /**< An ordinary app. */
        SYSTEM_SESSION_TYPE = 1 /**< A system session that requires elevated privileges.*/
    } SessionType;

    /** Models the running state of a process. */
    typedef enum
    {
        PROCESS_STOPPED = 0, /**< Process is stopped (SIGSTOP). */
        PROCESS_RUNNING = 1 /**< Process is running normally. */
    } RunningState;

    /**
     * Specifies whether the application tolerates a menubar when in fullscreen mode.
     * \ingroup ui_access
     */
    typedef enum
    {
        APPLICATION_SUPPORTS_OVERLAYED_MENUBAR = 0, /**< App allows for an overlayed menubar in fullscreen mode. */
        APPLICATION_DOES_NOT_SUPPORT_OVERLAYED_MENUBAR = 1 /**< App does not allow an overlayer menubar in fullscreen mode, i.e., a user pinning the menubar results in the app being maximized. */
    } MenuBarSupport;

    /**
     * Describes the different stages an application can run in.
     * \ingroup ui_access
     */
    typedef enum
    {
        MAIN_STAGE_HINT = 0, /**< Default stage, normal operation mode. */
        INTEGRATION_STAGE_HINT = 1, /**< Integration stage for reusing parts of an app's views. */
        SHARE_STAGE_HINT = 2, /**< Share stage for sharing data between apps. */
        CONTENT_PICKING_STAGE_HINT = 3, /**< Placeholder, not used yet. */
        SIDE_STAGE_HINT = 4, /**< Side stage for displaying apps alongside other apps running in the main stage. */
        CONFIGURATION_STAGE_HINT = 5 /**< Placeholder, not used yet. */
    } StageHint;

    /** 
     * Provides applications with a hint about the form factor it is running on. 
     * \ingroup ui_access
     */
    typedef enum
    {
        DESKTOP_FORM_FACTOR_HINT = 0, /**< An ordinary desktop or laptop form factor. */
        PHONE_FORM_FACTOR_HINT = 1, /**< A phone form factor. */
        TABLET_FORM_FACTOR_HINT = 2 /**< A tablet form factor. */
    } FormFactorHint;

    /**
     * Describes the role of a surface.
     * \ingroup ui_access
     * \attention Reserved roles require special privileges.
     */
    typedef enum
    {
        MAIN_ACTOR_ROLE = 1, /**< An application's main surface */
        DASH_ACTOR_ROLE = 0, /**< Reserved for the shell's dash */
        INDICATOR_ACTOR_ROLE = 2, /**< Reserved for the shell's indicators */
        NOTIFICATIONS_ACTOR_ROLE = 3, /**< Reserved for the shell's notifications */
        GREETER_ACTOR_ROLE = 4, /**< Reserved for the greeter */
        LAUNCHER_ACTOR_ROLE = 5, /**< Reserved for the launcher */
        ON_SCREEN_KEYBOARD_ACTOR_ROLE = 6, /**< Reserved for the onscreen-keyboard */
        SHUTDOWN_DIALOG_ACTOR_ROLE = 7 /**< Reserved for the shutdown dialog */
    } SurfaceRole;

    /**
     * Flags that can be specified for a surface 
     * \ingroup ui_access
     * \attention Requires privileged access to the ui service provider 
     */
    typedef enum
    {
        IS_OPAQUE_FLAG = 1
    } SurfaceFlags;

    enum
    {
        MAX_APPLICATION_NAME_LENGTH = 512
    };

    typedef void (*on_application_resumed)(void* ctx);
    typedef void (*on_application_suspended)(void* ctx);

    typedef void (*on_application_focused)(void* ctx);
    typedef void (*on_application_unfocused)(void* ctx);

    /**
     * Bundles information about an application instance.
     * \ingroup ui_access
     */    
    typedef struct
    {
        /** Specifies the session type */
        SessionType session_type;
        /** Per-application instance menu-bar support */
        MenuBarSupport menu_bar_support;
        /** The name of the application */
        char application_name[MAX_APPLICATION_NAME_LENGTH]; 

        // Lifecycle callbacks
        /** If set: called when the application is resumed. */
        on_application_resumed on_application_resumed_cb; 
        /** If set: called when the application is suspended. */
        on_application_suspended on_application_suspended_cb; 
        /** If set: called when the application is gaining focus. */
        on_application_focused on_application_focused_cb;
        /** If set: called when the application loses focus. */
        on_application_unfocused on_application_unfocused_cb; 
        /** Cookie that is passed to the callback functions. */
        void* context; 
    } SessionCredentials;

    typedef void (*input_event_cb)(void* ctx, const Event* ev);

    typedef void* ubuntu_application_ui_physical_display_info;
    typedef void* ubuntu_application_ui_session;
    typedef void* ubuntu_application_ui_surface;

    /** Initializes the Ubuntu Platform API. Has to be called before any other function is called.
     * \ingroup ui_access
     * \param argc [in] Argument count.
     * \param argv [in, out] Array of string arguments.
     */
    void
    ubuntu_application_ui_init(
        int argc,
        char**argv);

    /** 
     * Returns the stage this application instance is running in. 
     * \ingroup ui_access
     */
    StageHint
    ubuntu_application_ui_setup_get_stage_hint();

    /** 
     * Returns the form factor this application instance is running on. 
     * \ingroup ui_access
     */
    FormFactorHint
    ubuntu_application_ui_setup_get_form_factor_hint();

    /** 
     * Opens up a new session with the service providers.
     * \ingroup ui_access
     * \param creds [in] Credentials describing the instance's setup.
     */
    void
    ubuntu_application_ui_start_a_new_session(SessionCredentials* creds);

    /** 
     * Sets the clipboard content.
     * \ingroup ui_access 
     * \attention Needs an existing session.
     * \ingroup require_session_functions 
     */
    void
    ubuntu_application_ui_set_clipboard_content(
        void* data,
        size_t size); 

    /** 
     * Reads the clipboard content. 
     * \ingroup ui_access
     * \attention Needs an existing session.
     * \ingroup require_session_functions 
     */
    void
    ubuntu_application_ui_get_clipboard_content(
        void** data,
        size_t* size); 

    /** 
     * Queries information about a physical display with index index.
     * \ingroup ui_access
     * \attention Needs an existing session.
     * \ingroup require_session_functions 
     */
    void
    ubuntu_application_ui_create_display_info(
        ubuntu_application_ui_physical_display_info* info,
        size_t index);

    /** 
     * Destroy information about a physical display. 
     * \ingroup ui_access
     */
    void
    ubuntu_application_ui_destroy_display_info(
        ubuntu_application_ui_physical_display_info info);

    /** 
     * Queries the horizontal resolution in [px] of a physical display. 
     * \ingroup ui_access
     */
    int32_t
    ubuntu_application_ui_query_horizontal_resolution(
        ubuntu_application_ui_physical_display_info info);

    /** 
     * Queries the vertical resolution in [px] of a physical display. 
     * \ingroup ui_access
     */
    int32_t
    ubuntu_application_ui_query_vertical_resolution(
        ubuntu_application_ui_physical_display_info info);

    /** 
     * Queries the horizontal dpi of a physical display.
     * \ingroup ui_access
     */
    float
    ubuntu_application_ui_query_horizontal_dpi(
        ubuntu_application_ui_physical_display_info info);

    /** 
     * Queries the vertical dpi of a physical display.
     * \ingroup ui_access
     */
    float
    ubuntu_application_ui_query_vertical_dpi(
        ubuntu_application_ui_physical_display_info info);

    /** 
     * Creates a new surface and registers it for this application instance.
     * \attention Needs an existing session.
     * \ingroup require_session_functions 
     * \ingroup ui_access
     * \param out_surface [out] The resulting surface is placed here.
     * \param title [in] The title of the surface.
     * \param width [in] Requested width, will be clamped to available geometry.
     * \param height [in] Requested height, will be clamped to available geometry.
     * \param role [in] Role of the surface.
     * \param cb [in] Callback to be invoked for input events.
     * \param ctx [in] Context for the input event callback.
     */
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

    /** 
     * Requests to switch to fullscreen for the specified surface. 
     * \ingroup ui_access
     */
    void
    ubuntu_application_ui_request_fullscreen_for_surface(
        ubuntu_application_ui_surface surface);

    /** 
     * Destroys the specified surface.
     * \ingroup ui_access
     */
    void
    ubuntu_application_ui_destroy_surface(
        ubuntu_application_ui_surface surface);

    /** 
     * Translates a surface to an EGLNativeWindowType. 
     * \ingroup ui_access
     */
    EGLNativeWindowType
    ubuntu_application_ui_surface_to_native_window_type(
        ubuntu_application_ui_surface surface);

    /** 
     * Forces the specified surface to be shown on screen. 
     * \ingroup ui_access
     */
    void
    ubuntu_application_ui_show_surface(
        ubuntu_application_ui_surface surface);

    /** 
     * Forces the specified surface to be hidden. 
     * \ingroup ui_access
     */
    void
    ubuntu_application_ui_hide_surface(
        ubuntu_application_ui_surface surface);

    /** 
     * Moves the specified surface.
     * \ingroup ui_access
     * \param surface [in] The surface to be moved.
     * \param x [in] The new x coordinate.
     * \param y [in] The new y coordinate.
     */
    void
    ubuntu_application_ui_move_surface_to(
        ubuntu_application_ui_surface surface,
        int x,
        int y);

    /** 
     * Resizes the specified surface.
     * \ingroup ui_access
     * \param surface [in] The surface to be moved.
     * \param w [in] The new width of the surface.
     * \param h [in] The new height of the surface.
     */
    void
    ubuntu_application_ui_resize_surface_to(
        ubuntu_application_ui_surface surface,
        int w,
        int h);

    /** \example test_c_api.cpp */
    /** \example test_multiple_surfaces_event_delivery.cpp */

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_H_ */
