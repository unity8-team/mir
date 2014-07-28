/*
 * Copyright (C) 2014 Canonical Ltd
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

// C APIs
#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/ui/window.h>
#include <ubuntu/application/ui/options.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/clipboard.h>
#include <ubuntu/application/ui/display.h>
#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>
#include <ubuntu/application/sensors/orientation.h>
#include <ubuntu/application/sensors/haptic.h>

#include <ubuntu/application/location/service.h>
#include <ubuntu/application/location/heading_update.h>
#include <ubuntu/application/location/position_update.h>
#include <ubuntu/application/location/velocity_update.h>

#include <ubuntu/application/url_dispatcher/service.h>

#include <ubuntu/application/init.h>

#include "base_module.h"

#ifdef __cplusplus
extern "C" {
#endif

// Application Module Config
IMPLEMENT_VOID_FUNCTION3(u_application_module_version, uint32_t*, uint32_t*, uint32_t*);
IMPLEMENT_VOID_FUNCTION1(u_application_init, void*);
IMPLEMENT_VOID_FUNCTION0(u_application_finish);

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

// Ubuntu Application Sensors

// Acceleration Sensor
IMPLEMENT_CTOR0(UASensorsAccelerometer*, ua_sensors_accelerometer_new);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_accelerometer_enable, UASensorsAccelerometer*);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_accelerometer_disable, UASensorsAccelerometer*);
IMPLEMENT_FUNCTION1(uint32_t, ua_sensors_accelerometer_get_min_delay, UASensorsAccelerometer*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_accelerometer_get_min_value, UASensorsAccelerometer*, float*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_accelerometer_get_max_value, UASensorsAccelerometer*, float*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_accelerometer_get_resolution, UASensorsAccelerometer*, float*);
IMPLEMENT_VOID_FUNCTION3(ua_sensors_accelerometer_set_reading_cb, UASensorsAccelerometer*, on_accelerometer_event_cb, void*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_accelerometer_set_event_rate, UASensorsAccelerometer*, uint32_t);

// Acceleration Sensor Event
IMPLEMENT_FUNCTION1(uint64_t, uas_accelerometer_event_get_timestamp, UASAccelerometerEvent*);
IMPLEMENT_FUNCTION2(UStatus, uas_accelerometer_event_get_acceleration_x, UASAccelerometerEvent*, float*);
IMPLEMENT_FUNCTION2(UStatus, uas_accelerometer_event_get_acceleration_y, UASAccelerometerEvent*, float*);
IMPLEMENT_FUNCTION2(UStatus, uas_accelerometer_event_get_acceleration_z, UASAccelerometerEvent*, float*);

// Proximity Sensor
IMPLEMENT_CTOR0(UASensorsProximity*, ua_sensors_proximity_new);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_proximity_enable, UASensorsProximity*);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_proximity_disable, UASensorsProximity*);
IMPLEMENT_FUNCTION1(uint32_t, ua_sensors_proximity_get_min_delay, UASensorsProximity*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_proximity_get_min_value, UASensorsProximity*, float*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_proximity_get_max_value, UASensorsProximity*, float*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_proximity_get_resolution, UASensorsProximity*, float*);
IMPLEMENT_VOID_FUNCTION3(ua_sensors_proximity_set_reading_cb, UASensorsProximity*, on_proximity_event_cb, void*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_proximity_set_event_rate, UASensorsProximity*, uint32_t);

// Proximity Sensor Event
IMPLEMENT_FUNCTION1(uint64_t, uas_proximity_event_get_timestamp, UASProximityEvent*);
IMPLEMENT_FUNCTION1(UASProximityDistance, uas_proximity_event_get_distance, UASProximityEvent*);

// Ambient Light Sensor
IMPLEMENT_CTOR0(UASensorsLight*, ua_sensors_light_new);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_light_enable, UASensorsLight*);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_light_disable, UASensorsLight*);
IMPLEMENT_FUNCTION1(uint32_t, ua_sensors_light_get_min_delay, UASensorsLight*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_light_get_min_value, UASensorsLight*, float*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_light_get_max_value, UASensorsLight*, float*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_light_get_resolution, UASensorsLight*, float*);
IMPLEMENT_VOID_FUNCTION3(ua_sensors_light_set_reading_cb, UASensorsLight*, on_light_event_cb, void*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_light_set_event_rate, UASensorsLight*, uint32_t);

// Ambient Light Sensor Event
IMPLEMENT_FUNCTION1(uint64_t, uas_light_event_get_timestamp, UASLightEvent*);
IMPLEMENT_FUNCTION2(UStatus, uas_light_event_get_light, UASLightEvent*, float*);

// Haptic Sensor
IMPLEMENT_CTOR0(UASensorsHaptic*, ua_sensors_haptic_new);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_haptic_enable, UASensorsHaptic*);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_haptic_disable, UASensorsHaptic*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_haptic_vibrate_once, UASensorsHaptic*, uint32_t);
IMPLEMENT_FUNCTION3(UStatus, ua_sensors_haptic_vibrate_with_pattern, UASensorsHaptic*, uint32_t*, uint32_t);

// Orientation Sensor
IMPLEMENT_CTOR0(UASensorsOrientation*, ua_sensors_orientation_new);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_orientation_enable, UASensorsOrientation*);
IMPLEMENT_FUNCTION1(UStatus, ua_sensors_orientation_disable, UASensorsOrientation*);
IMPLEMENT_FUNCTION1(uint32_t, ua_sensors_orientation_get_min_delay, UASensorsOrientation*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_orientation_get_min_value, UASensorsOrientation*, float*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_orientation_get_max_value, UASensorsOrientation*, float*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_orientation_get_resolution, UASensorsOrientation*, float*);
IMPLEMENT_VOID_FUNCTION3(ua_sensors_orientation_set_reading_cb, UASensorsOrientation*, on_orientation_event_cb, void*);
IMPLEMENT_FUNCTION2(UStatus, ua_sensors_orientation_set_event_rate, UASensorsOrientation*, uint32_t);

// Orientation Sensor Event
IMPLEMENT_FUNCTION1(uint64_t, uas_orientation_event_get_timestamp, UASOrientationEvent*);
IMPLEMENT_FUNCTION2(UStatus, uas_orientation_event_get_azimuth, UASOrientationEvent*, float*);
IMPLEMENT_FUNCTION2(UStatus, uas_orientation_event_get_pitch, UASOrientationEvent*, float*);
IMPLEMENT_FUNCTION2(UStatus, uas_orientation_event_get_roll, UASOrientationEvent*, float*);

// Location

IMPLEMENT_VOID_FUNCTION1(ua_location_service_controller_ref, UALocationServiceController*);
IMPLEMENT_VOID_FUNCTION1(ua_location_service_controller_unref, UALocationServiceController*);
IMPLEMENT_VOID_FUNCTION3(ua_location_service_controller_set_status_changed_handler, UALocationServiceController*, UALocationServiceStatusChangedHandler, void*);
IMPLEMENT_FUNCTION2(UStatus, ua_location_service_controller_query_status, UALocationServiceController*, UALocationServiceStatusFlags*);
IMPLEMENT_FUNCTION1(UStatus, ua_location_service_controller_enable_service, UALocationServiceController*);
IMPLEMENT_FUNCTION1(UStatus, ua_location_service_controller_disable_service, UALocationServiceController*);
IMPLEMENT_FUNCTION1(UStatus, ua_location_service_controller_enable_gps, UALocationServiceController*);
IMPLEMENT_FUNCTION1(UStatus, ua_location_service_controller_disable_gps, UALocationServiceController*);
IMPLEMENT_VOID_FUNCTION1(ua_location_heading_update_ref, UALocationHeadingUpdate*);
IMPLEMENT_VOID_FUNCTION1(ua_location_heading_update_unref, UALocationHeadingUpdate*);
IMPLEMENT_FUNCTION1(uint64_t, ua_location_heading_update_get_timestamp, UALocationHeadingUpdate*);
IMPLEMENT_FUNCTION1(double, ua_location_heading_update_get_heading_in_degree, UALocationHeadingUpdate*);
IMPLEMENT_VOID_FUNCTION1(ua_location_position_update_ref, UALocationPositionUpdate*);
IMPLEMENT_VOID_FUNCTION1(ua_location_position_update_unref, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(uint64_t, ua_location_position_update_get_timestamp, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(double, ua_location_position_update_get_latitude_in_degree, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(double, ua_location_position_update_get_longitude_in_degree, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(bool, ua_location_position_update_has_altitude, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(double, ua_location_position_update_get_altitude_in_meter, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(bool, ua_location_position_update_has_horizontal_accuracy, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(double, ua_location_position_update_get_horizontal_accuracy_in_meter, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(bool, ua_location_position_update_has_vertical_accuracy, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(double, ua_location_position_update_get_vertical_accuracy_in_meter, UALocationPositionUpdate*);
IMPLEMENT_FUNCTION1(UALocationServiceSession*, ua_location_service_create_session_for_low_accuracy, UALocationServiceRequirementsFlags);
IMPLEMENT_FUNCTION1(UALocationServiceSession*, ua_location_service_create_session_for_high_accuracy, UALocationServiceRequirementsFlags);
IMPLEMENT_CTOR0(UALocationServiceController*, ua_location_service_create_controller);
IMPLEMENT_VOID_FUNCTION1(ua_location_service_session_ref, UALocationServiceSession*);
IMPLEMENT_VOID_FUNCTION1(ua_location_service_session_unref, UALocationServiceSession*);
IMPLEMENT_VOID_FUNCTION3(ua_location_service_session_set_position_updates_handler, UALocationServiceSession*, UALocationServiceSessionPositionUpdatesHandler, void*);
IMPLEMENT_VOID_FUNCTION3(ua_location_service_session_set_heading_updates_handler, UALocationServiceSession*, UALocationServiceSessionHeadingUpdatesHandler, void*);
IMPLEMENT_VOID_FUNCTION3(ua_location_service_session_set_velocity_updates_handler, UALocationServiceSession*, UALocationServiceSessionVelocityUpdatesHandler, void*);
IMPLEMENT_FUNCTION1(UStatus, ua_location_service_session_start_position_updates, UALocationServiceSession*);
IMPLEMENT_VOID_FUNCTION1(ua_location_service_session_stop_position_updates, UALocationServiceSession*);
IMPLEMENT_FUNCTION1(UStatus, ua_location_service_session_start_heading_updates, UALocationServiceSession*);
IMPLEMENT_VOID_FUNCTION1(ua_location_service_session_stop_heading_updates, UALocationServiceSession*);
IMPLEMENT_FUNCTION1(UStatus, ua_location_service_session_start_velocity_updates, UALocationServiceSession*);
IMPLEMENT_VOID_FUNCTION1(ua_location_service_session_stop_velocity_updates, UALocationServiceSession*);
IMPLEMENT_VOID_FUNCTION1(ua_location_velocity_update_ref, UALocationVelocityUpdate*);
IMPLEMENT_VOID_FUNCTION1(ua_location_velocity_update_unref, UALocationVelocityUpdate*);
IMPLEMENT_FUNCTION1(uint64_t, ua_location_velocity_update_get_timestamp, UALocationVelocityUpdate*);
IMPLEMENT_FUNCTION1(double, ua_location_velocity_update_get_velocity_in_meters_per_second, UALocationVelocityUpdate*);

// URL Dispatcher

IMPLEMENT_CTOR0(UAUrlDispatcherSession*, ua_url_dispatcher_session);
IMPLEMENT_VOID_FUNCTION4(ua_url_dispatcher_session_open, UAUrlDispatcherSession*, const char*, UAUrlDispatcherSessionDispatchHandler, void*);

#ifdef __cplusplus
}
#endif
