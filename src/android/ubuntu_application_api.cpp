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
 */

#include <private/application/ui/ubuntu_application_ui.h>

#include <ubuntu/application/sensors/ubuntu_application_sensors.h>
#include <ubuntu/application/ubuntu_application_gps.h>
#include <ubuntu/ui/ubuntu_ui_session_service.h>
#include <ubuntu/application/lifecycle_delegate.h>

// C APIs
#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/ui/window.h>
#include <ubuntu/application/ui/options.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/clipboard.h>
#include <ubuntu/application/ui/display.h>

#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *android_dlopen(const char *filename, int flag);
extern void *android_dlsym(void *handle, const char *symbol);

#ifdef __cplusplus
}
#endif

namespace
{

struct Bridge
{
    static const char* path_to_library()
    {
        return "/system/lib/libubuntu_application_api.so";
    }

    static Bridge& instance() { static Bridge bridge; return bridge; }
    
    Bridge() : lib_handle(android_dlopen(path_to_library(), RTLD_LAZY))
    {
        assert(lib_handle && "Error loading ubuntu_application_api");
    }

    ~Bridge()
    {
        // TODO android_dlclose(libcamera_handle);
    }

    void* resolve_symbol(const char* symbol) const
    {
        return android_dlsym(lib_handle, symbol);
    }
    
    void* lib_handle;
};

}

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************/
/*********** Implementation starts here *******************/
/**********************************************************/

#define DLSYM(fptr, sym) if (*(fptr) == NULL) { *((void**)fptr) = (void *) Bridge::instance().resolve_symbol(sym); }
    
#define IMPLEMENT_FUNCTION0(return_type, symbol)  \
    return_type symbol()                          \
    {                                             \
        static return_type (*f)() = NULL;         \
        DLSYM(&f, #symbol);                       \
        return f();}

#define IMPLEMENT_VOID_FUNCTION0(symbol)          \
    void symbol()                                 \
    {                                             \
        static void (*f)() = NULL;                \
        DLSYM(&f, #symbol);                       \
        f();}
    
#define IMPLEMENT_FUNCTION1(return_type, symbol, arg1) \
    return_type symbol(arg1 _1)                        \
    {                                                  \
        static return_type (*f)(arg1) = NULL;          \
        DLSYM(&f, #symbol);                     \
        return f(_1); }

#define IMPLEMENT_VOID_FUNCTION1(symbol, arg1)               \
    void symbol(arg1 _1)                                     \
    {                                                        \
        static void (*f)(arg1) = NULL;                       \
        DLSYM(&f, #symbol);                           \
        f(_1); }

#define IMPLEMENT_FUNCTION2(return_type, symbol, arg1, arg2)    \
    return_type symbol(arg1 _1, arg2 _2)                        \
    {                                                           \
        static return_type (*f)(arg1, arg2) = NULL;             \
        DLSYM(&f, #symbol);                              \
        return f(_1, _2); }

#define IMPLEMENT_VOID_FUNCTION2(symbol, arg1, arg2)            \
    void symbol(arg1 _1, arg2 _2)                               \
    {                                                           \
        static void (*f)(arg1, arg2) = NULL;                    \
        DLSYM(&f, #symbol);                              \
        f(_1, _2); }

#define IMPLEMENT_FUNCTION3(return_type, symbol, arg1, arg2, arg3)    \
    return_type symbol(arg1 _1, arg2 _2, arg3 _3)                     \
    {                                                                 \
        static return_type (*f)(arg1, arg2, arg3) = NULL;             \
        DLSYM(&f, #symbol);                                           \
        return f(_1, _2, _3); } 

#define IMPLEMENT_VOID_FUNCTION3(symbol, arg1, arg2, arg3)      \
    void symbol(arg1 _1, arg2 _2, arg3 _3)                      \
    {                                                           \
        static void (*f)(arg1, arg2, arg3) = NULL;              \
        DLSYM(&f, #symbol);                                     \
        f(_1, _2, _3); }

#define IMPLEMENT_VOID_FUNCTION4(symbol, arg1, arg2, arg3, arg4) \
    void symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4)              \
    {                                                            \
        static void (*f)(arg1, arg2, arg3, arg4) = NULL;         \
        DLSYM(&f, #symbol);                                      \
        f(_1, _2, _3, _4); }

#define IMPLEMENT_FUNCTION4(return_type, symbol, arg1, arg2, arg3, arg4) \
    return_type symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4)               \
    {                                                                    \
        static return_type (*f)(arg1, arg2, arg3, arg4) = NULL;          \
        DLSYM(&f, #symbol);                                              \
        return f(_1, _2, _3, _4); }

#define IMPLEMENT_FUNCTION6(return_type, symbol, arg1, arg2, arg3, arg4, arg5, arg6) \
    return_type symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4, arg5 _5, arg6 _6)         \
    {                                                                                \
        static return_type (*f)(arg1, arg2, arg3, arg4, arg5, arg6) = NULL;          \
        DLSYM(&f, #symbol);                                                          \
        return f(_1, _2, _3, _4, _5, _6); }

#define IMPLEMENT_VOID_FUNCTION7(symbol, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    void symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4, arg5 _5, arg6 _6, arg7 _7) \
    {                                                                   \
        static void (*f)(arg1, arg2, arg3, arg4, arg5, arg6, arg7) = NULL; \
        DLSYM(&f, #symbol);                                             \
        f(_1, _2, _3, _4, _5, _6, _7); }

#define IMPLEMENT_VOID_FUNCTION8(symbol, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    void symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4, arg5 _5, arg6 _6, arg7 _7, arg8 _8) \
    {                                                                   \
        static void (*f)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) = NULL; \
        DLSYM(&f, #symbol);                                             \
        f(_1, _2, _3, _4, _5, _6, _7, _8); }

// Session helpers
IMPLEMENT_FUNCTION0(UAUiSessionProperties*, ua_ui_session_properties_new);
IMPLEMENT_VOID_FUNCTION2(ua_ui_session_properties_set_type, UAUiSessionProperties*, UAUiSessionType);
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


// UAUiWindowProperties
IMPLEMENT_FUNCTION0(UAUiWindowProperties*, ua_ui_window_properties_new_for_normal_window);
IMPLEMENT_VOID_FUNCTION1(ua_ui_window_properties_destroy, UAUiWindowProperties*);
IMPLEMENT_VOID_FUNCTION3(ua_ui_window_properties_set_titlen, UAUiWindowProperties*, const char*, size_t);
IMPLEMENT_FUNCTION1(const char*, ua_ui_window_properties_get_title, UAUiWindowProperties*);
IMPLEMENT_VOID_FUNCTION2(ua_ui_window_properties_set_role, UAUiWindowProperties*, UAUiWindowRole);
IMPLEMENT_VOID_FUNCTION3(ua_ui_window_properties_set_input_cb_and_ctx, UAUiWindowProperties*, UAUiWindowInputEventCb, void*);

// UAUiWindow
IMPLEMENT_FUNCTION2(UAUiWindow*, ua_ui_window_new_for_application_with_properties, UApplicationInstance*, UAUiWindowProperties*);
IMPLEMENT_VOID_FUNCTION1(ua_ui_window_destroy, UAUiWindow*);
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

// Sensor service
IMPLEMENT_VOID_FUNCTION1(ubuntu_sensor_initialize_observer, ubuntu_sensor_observer*);
IMPLEMENT_VOID_FUNCTION1(ubuntu_sensor_install_observer, ubuntu_sensor_observer*);
IMPLEMENT_VOID_FUNCTION1(ubuntu_sensor_uninstall_observer, ubuntu_sensor_observer*);
IMPLEMENT_VOID_FUNCTION1(ubuntu_sensor_enable_sensor, ubuntu_sensor_type);
IMPLEMENT_VOID_FUNCTION1(ubuntu_sensor_disable_sensor, ubuntu_sensor_type);
IMPLEMENT_FUNCTION1(int32_t, ubuntu_sensor_get_sensor_min_delay, ubuntu_sensor_type);
IMPLEMENT_FUNCTION1(float, ubuntu_sensor_get_sensor_min_value, ubuntu_sensor_type);
IMPLEMENT_FUNCTION1(float, ubuntu_sensor_get_sensor_max_value, ubuntu_sensor_type);
IMPLEMENT_FUNCTION1(float, ubuntu_sensor_get_sensor_resolution, ubuntu_sensor_type);

// GPS
IMPLEMENT_FUNCTION1(UbuntuGps, ubuntu_gps_new, UbuntuGpsParams*);
IMPLEMENT_VOID_FUNCTION1(ubuntu_gps_delete, UbuntuGps);
IMPLEMENT_FUNCTION1(bool, ubuntu_gps_start, UbuntuGps);
IMPLEMENT_FUNCTION1(bool, ubuntu_gps_stop, UbuntuGps);
IMPLEMENT_VOID_FUNCTION4(ubuntu_gps_inject_time, UbuntuGps, int64_t, int64_t, int);
IMPLEMENT_VOID_FUNCTION4(ubuntu_gps_inject_location, UbuntuGps, double, double, float);
IMPLEMENT_VOID_FUNCTION2(ubuntu_gps_delete_aiding_data, UbuntuGps, uint16_t);
IMPLEMENT_FUNCTION6(bool, ubuntu_gps_set_position_mode, UbuntuGps, uint32_t, uint32_t,
                                                        uint32_t, uint32_t, uint32_t);
IMPLEMENT_VOID_FUNCTION3(ubuntu_gps_inject_xtra_data, UbuntuGps, char*, int);

#ifdef __cplusplus
}
#endif
