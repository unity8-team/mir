/*
 * Copyright (C) 2012 Canonical Ltd
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 *              Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#include <private/application/ui/ubuntu_application_ui.h>

#include <ubuntu/ui/ubuntu_ui_session_service.h>

// C APIs
#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/ui/window.h>
#include <ubuntu/application/ui/options.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/clipboard.h>
#include <ubuntu/application/ui/display.h>

#include "hybris_module.h"

#ifdef __cplusplus
extern "C" {
#endif

// Session helpers
IMPLEMENT_FUNCTION0(UAUiSessionProperties*, ua_ui_session_properties_new);
IMPLEMENT_VOID_FUNCTION2(ua_ui_session_properties_set_type, UAUiSessionProperties*, UAUiSessionType);
IMPLEMENT_VOID_FUNCTION2(ua_ui_session_properties_set_remote_pid, UAUiSessionProperties*, uint32_t);
IMPLEMENT_FUNCTION1(UAUiSession*, ua_ui_session_new_with_properties, UAUiSessionProperties*);

// Lifecycle helpers
IMPLEMENT_FUNCTION0(UApplicationLifecycleDelegate*, u_application_lifecycle_delegate_new);
IMPLEMENT_VOID_FUNCTION2(u_application_lifecycle_delegate_set_context, UApplicationLifecycleDelegate*, void*);
IMPLEMENT_VOID_FUNCTION1(u_application_lifecycle_delegate_ref, UApplicationLifecycleDelegate*);
IMPLEMENT_VOID_FUNCTION1(u_application_lifecycle_delegate_unref, UApplicationLifecycleDelegate*);
IMPLEMENT_VOID_FUNCTION2(u_application_lifecycle_delegate_set_application_resumed_cb, UApplicationLifecycleDelegate*, u_on_application_resumed);
IMPLEMENT_VOID_FUNCTION2(u_application_lifecycle_delegate_set_application_about_to_stop_cb, UApplicationLifecycleDelegate*, u_on_application_about_to_stop);

// Application Helpers

// UApplicationId
IMPLEMENT_FUNCTION2(UApplicationId*, u_application_id_new_from_stringn, const char*, size_t);
IMPLEMENT_VOID_FUNCTION1(u_application_id_destroy, UApplicationId*);
IMPLEMENT_FUNCTION2(int, u_application_id_compare, UApplicationId*, UApplicationId*);

// UApplicationDescription
IMPLEMENT_FUNCTION0(UApplicationDescription*, u_application_description_new);
IMPLEMENT_VOID_FUNCTION1(u_application_description_destroy, UApplicationDescription*);
IMPLEMENT_VOID_FUNCTION2(u_application_description_set_application_id, UApplicationDescription*, UApplicationId*);
IMPLEMENT_VOID_FUNCTION2(u_application_description_set_application_lifecycle_delegate, UApplicationDescription*, UApplicationLifecycleDelegate*);

// UApplicationOptions
IMPLEMENT_FUNCTION2(UApplicationOptions*, u_application_options_new_from_cmd_line, int, char**);
IMPLEMENT_VOID_FUNCTION1(u_application_options_destroy, UApplicationOptions*);
IMPLEMENT_FUNCTION1(UAUiFormFactor, u_application_options_get_form_factor, UApplicationOptions*);
IMPLEMENT_FUNCTION1(UAUiStage, u_application_options_get_stage, UApplicationOptions*);

// UApplicationInstance
IMPLEMENT_FUNCTION2(UApplicationInstance*, u_application_instance_new_from_description_with_options, UApplicationDescription*, UApplicationOptions*);

// Application Ui Helpers

// Clipboard
IMPLEMENT_VOID_FUNCTION2(ua_ui_set_clipboard_content, void*, size_t);
IMPLEMENT_VOID_FUNCTION2(ua_ui_get_clipboard_content, void**, size_t*);

// UAUiDisplay
IMPLEMENT_FUNCTION1(UAUiDisplay*, ua_ui_display_new_with_index, size_t);
IMPLEMENT_VOID_FUNCTION1(ua_ui_display_destroy, UAUiDisplay*);
IMPLEMENT_FUNCTION1(uint32_t, ua_ui_display_query_horizontal_res, UAUiDisplay*);
IMPLEMENT_FUNCTION1(uint32_t, ua_ui_display_query_vertical_res, UAUiDisplay*);
IMPLEMENT_FUNCTION1(EGLNativeDisplayType, ua_ui_display_get_native_type, UAUiDisplay*);


// UAUiWindowProperties
IMPLEMENT_FUNCTION0(UAUiWindowProperties*, ua_ui_window_properties_new_for_normal_window);
IMPLEMENT_VOID_FUNCTION1(ua_ui_window_properties_destroy, UAUiWindowProperties*);
IMPLEMENT_VOID_FUNCTION3(ua_ui_window_properties_set_titlen, UAUiWindowProperties*, const char*, size_t);
IMPLEMENT_FUNCTION1(const char*, ua_ui_window_properties_get_title, UAUiWindowProperties*);
IMPLEMENT_VOID_FUNCTION2(ua_ui_window_properties_set_role, UAUiWindowProperties*, UAUiWindowRole);
IMPLEMENT_VOID_FUNCTION3(ua_ui_window_properties_set_input_cb_and_ctx, UAUiWindowProperties*, UAUiWindowInputEventCb, void*);
IMPLEMENT_VOID_FUNCTION3(ua_ui_window_properties_set_event_cb_and_ctx, UAUiWindowProperties*, UAUiWindowEventCb, void*);
IMPLEMENT_VOID_FUNCTION3(ua_ui_window_properties_set_dimensions, UAUiWindowProperties*, uint32_t, uint32_t);

// UAUiWindow
IMPLEMENT_FUNCTION2(UAUiWindow*, ua_ui_window_new_for_application_with_properties, UApplicationInstance*, UAUiWindowProperties*);
IMPLEMENT_VOID_FUNCTION1(ua_ui_window_destroy, UAUiWindow*);
IMPLEMENT_VOID_FUNCTION3(ua_ui_window_get_size, UAUiWindow*, uint32_t*, uint32_t*);
IMPLEMENT_FUNCTION1(int, ua_ui_window_is_focused, UAUiWindow*);
IMPLEMENT_FUNCTION3(UStatus, ua_ui_window_move, UAUiWindow*, uint32_t, uint32_t);
IMPLEMENT_FUNCTION3(UStatus, ua_ui_window_resize, UAUiWindow*, uint32_t, uint32_t);
IMPLEMENT_FUNCTION1(UStatus, ua_ui_window_hide, UAUiWindow*);
IMPLEMENT_FUNCTION1(UStatus, ua_ui_window_show, UAUiWindow*);
IMPLEMENT_VOID_FUNCTION1(ua_ui_window_request_fullscreen, UAUiWindow*);
IMPLEMENT_FUNCTION1(EGLNativeWindowType, ua_ui_window_get_native_type, UAUiWindow*);

/* -------------------------------------------------------------------------- *
 * * * * * * * * * * * * * * * Deprecated API * * * * * * * * * * * * * * * * *
 * -------------------------------------------------------------------------- *
 */

// Session service
IMPLEMENT_FUNCTION1(int, ubuntu_ui_session_properties_get_application_instance_id, ubuntu_ui_session_properties);
IMPLEMENT_FUNCTION1(int, ubuntu_ui_session_properties_get_application_stage_hint, ubuntu_ui_session_properties);
IMPLEMENT_FUNCTION1(const char*, ubuntu_ui_session_properties_get_desktop_file_hint, ubuntu_ui_session_properties);
IMPLEMENT_VOID_FUNCTION1(ubuntu_ui_session_install_session_lifecycle_observer, ubuntu_ui_session_lifecycle_observer*);
IMPLEMENT_VOID_FUNCTION0(ubuntu_ui_session_unfocus_running_sessions);
IMPLEMENT_VOID_FUNCTION1(ubuntu_ui_session_focus_running_session_with_id, int);
IMPLEMENT_VOID_FUNCTION3(ubuntu_ui_session_snapshot_running_session_with_id, int, ubuntu_ui_session_service_snapshot_cb, void*); 
IMPLEMENT_VOID_FUNCTION1(ubuntu_ui_session_trigger_switch_to_well_known_application, ubuntu_ui_well_known_application);
IMPLEMENT_FUNCTION4(int32_t, ubuntu_ui_set_surface_trap, int, int, int, int);
IMPLEMENT_VOID_FUNCTION1(ubuntu_ui_unset_surface_trap, int);
IMPLEMENT_VOID_FUNCTION4(ubuntu_ui_report_osk_visible, int, int, int, int);
IMPLEMENT_VOID_FUNCTION0(ubuntu_ui_report_osk_invisible);
IMPLEMENT_VOID_FUNCTION0(ubuntu_ui_report_notification_visible);
IMPLEMENT_VOID_FUNCTION0(ubuntu_ui_report_notification_invisible);
IMPLEMENT_VOID_FUNCTION1(ubuntu_ui_install_task_controller, ubuntu_ui_task_controller*);

#ifdef __cplusplus
}
#endif
