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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>
#include <ubuntu/application/sensors/orientation.h>

#include <stddef.h>

// Ubuntu Application Sensors. Null desktop implementation

// Acceleration Sensor
UASensorsAccelerometer* ua_sensors_accelerometer_new()
{
    return NULL;
}

UStatus ua_sensors_accelerometer_enable(UASensorsAccelerometer*)
{
    return (UStatus) 0;
}

UStatus ua_sensors_accelerometer_disable(UASensorsAccelerometer*)
{
    return (UStatus) 0;
}

uint32_t ua_sensors_accelerometer_get_min_delay(UASensorsAccelerometer*)
{
    return 0;
}

UStatus ua_sensors_accelerometer_get_min_value(UASensorsAccelerometer*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_accelerometer_get_max_value(UASensorsAccelerometer*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_accelerometer_get_resolution(UASensorsAccelerometer*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_accelerometer_set_event_rate(UASensorsAccelerometer* s, uint32_t rate)
{
    return U_STATUS_SUCCESS;
}

void ua_sensors_accelerometer_set_reading_cb(UASensorsAccelerometer*, on_accelerometer_event_cb, void*)
{
}

// Acceleration Sensor Event
uint64_t uas_accelerometer_event_get_timestamp(UASAccelerometerEvent*)
{
    return 0;
}

UStatus uas_accelerometer_event_get_acceleration_x(UASAccelerometerEvent*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus uas_accelerometer_event_get_acceleration_y(UASAccelerometerEvent*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus uas_accelerometer_event_get_acceleration_z(UASAccelerometerEvent*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

// Proximity Sensor
UASensorsProximity* ua_sensors_proximity_new()
{
    return NULL;
}

UStatus ua_sensors_proximity_enable(UASensorsProximity*)
{
    return (UStatus) 0;
}

UStatus ua_sensors_proximity_disable(UASensorsProximity*)
{
    return (UStatus) 0;
}

uint32_t ua_sensors_proximity_get_min_delay(UASensorsProximity*)
{
    return 0;
}

UStatus ua_sensors_proximity_get_min_value(UASensorsProximity*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_proximity_get_max_value(UASensorsProximity*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_proximity_get_resolution(UASensorsProximity*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_proximity_set_event_rate(UASensorsProximity* s, uint32_t rate)
{
    return U_STATUS_SUCCESS;
}

void ua_sensors_proximity_set_reading_cb(UASensorsProximity*, on_proximity_event_cb, void*)
{
}

// Proximity Sensor Event
uint64_t uas_proximity_event_get_timestamp(UASProximityEvent*)
{
    return 0;
}

UASProximityDistance uas_proximity_event_get_distance(UASProximityEvent*)
{
    return (UASProximityDistance) 0;
}


// Ambient Light Sensor
UASensorsLight* ua_sensors_light_new()
{
    return NULL;
}

UStatus ua_sensors_light_enable(UASensorsLight*)
{
    return (UStatus) 0;
}

UStatus ua_sensors_light_disable(UASensorsLight*)
{
    return (UStatus) 0;
}

uint32_t ua_sensors_light_get_min_delay(UASensorsLight*)
{
    return 0;
}

UStatus ua_sensors_light_get_min_value(UASensorsLight*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_light_get_max_value(UASensorsLight*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_light_get_resolution(UASensorsLight*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_light_set_event_rate(UASensorsLight* s, uint32_t rate)
{
    return U_STATUS_SUCCESS;
}

void ua_sensors_light_set_reading_cb(UASensorsLight*, on_light_event_cb, void*)
{
}

// Ambient Light Sensor Event
uint64_t uas_light_event_get_timestamp(UASLightEvent*)
{
    return 0;
}

UStatus uas_light_event_get_light(UASLightEvent*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

// Orientation Sensor
UASensorsOrientation* ua_sensors_orientation_new()
{
    return NULL;
}

UStatus ua_sensors_orientation_enable(UASensorsOrientation*)
{
    return (UStatus) 0;
}

UStatus ua_sensors_orientation_disable(UASensorsOrientation*)
{
    return (UStatus) 0;
}

uint32_t ua_sensors_orientation_get_min_delay(UASensorsOrientation*)
{
    return 0;
}

UStatus ua_sensors_orientation_get_min_value(UASensorsOrientation*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_orientation_get_max_value(UASensorsOrientation*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_orientation_get_resolution(UASensorsOrientation*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus ua_sensors_orientation_set_event_rate(UASensorsOrientation* s, uint32_t rate)
{
    return U_STATUS_SUCCESS;
}

void ua_sensors_orientation_set_reading_cb(UASensorsOrientation*, on_orientation_event_cb, void*)
{
}

// Orientation Sensor Event
uint64_t uas_orientation_event_get_timestamp(UASOrientationEvent*)
{
    return 0;
}

UStatus uas_orientation_event_get_azimuth(UASOrientationEvent*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus uas_orientation_event_get_pitch(UASOrientationEvent*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}

UStatus uas_orientation_event_get_roll(UASOrientationEvent*, float* value)
{
    if (!value)
        return U_STATUS_ERROR;

    *value = 0.f;

    return U_STATUS_SUCCESS;
}
