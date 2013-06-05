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

#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

// Ubuntu Application Sensors. Null desktop implementation

// Acceleration Sensor
UASensorsAccelerometer* ua_sensors_accelerometer_new()
{
    return NULL;
}

UStatus ua_sensors_accelerometer_enable(UASensorsAccelerometer*)
{
}

UStatus ua_sensors_accelerometer_disable(UASensorsAccelerometer*)
{
}

uint32_t ua_sensors_accelerometer_get_min_delay(UASensorsAccelerometer*)
{
}

float ua_sensors_accelerometer_get_min_value(UASensorsAccelerometer*)
{
}

float ua_sensors_accelerometer_get_max_value(UASensorsAccelerometer*)
{
}

float ua_sensors_accelerometer_get_resolution(UASensorsAccelerometer*)
{
}

void ua_sensors_accelerometer_set_reading_cb(UASensorsAccelerometer*, on_accelerometer_event_cb, void*)
{
}

// Acceleration Sensor Event
uint64_t uas_accelerometer_event_get_timestamp(UASAccelerometerEvent*)
{
}

float uas_accelerometer_event_get_acceleration_x(UASAccelerometerEvent*)
{
}

float uas_accelerometer_event_get_acceleration_y(UASAccelerometerEvent*)
{
}

float uas_accelerometer_event_get_acceleration_z(UASAccelerometerEvent*)
{
}

// Proximity Sensor
UASensorsProximity* ua_sensors_proximity_new()
{
}

UStatus ua_sensors_proximity_enable(UASensorsProximity*)
{
}

UStatus ua_sensors_proximity_disable(UASensorsProximity*)
{
}

uint32_t ua_sensors_proximity_get_min_delay(UASensorsProximity*)
{
}

float ua_sensors_proximity_get_min_value(UASensorsProximity*)
{
}

float ua_sensors_proximity_get_max_value(UASensorsProximity*)
{
}

float ua_sensors_proximity_get_resolution(UASensorsProximity*)
{
}

void ua_sensors_proximity_set_reading_cb(UASensorsProximity*, on_proximity_event_cb, void*)
{
}

// Proximity Sensor Event
uint64_t uas_proximity_event_get_timestamp(UASProximityEvent*)
{
}

UASProximityDistance uas_proximity_event_get_distance(UASProximityEvent*)
{
}


// Ambient Light Sensor
UASensorsLight* ua_sensors_light_new()
{
}

UStatus ua_sensors_light_enable(UASensorsLight*)
{
}

UStatus ua_sensors_light_disable(UASensorsLight*)
{
}

uint32_t ua_sensors_light_get_min_delay(UASensorsLight*)
{
}

float ua_sensors_light_get_min_value(UASensorsLight*)
{
}

float ua_sensors_light_get_max_value(UASensorsLight*)
{
}

float ua_sensors_light_get_resolution(UASensorsLight*)
{
}

void ua_sensors_light_set_reading_cb(UASensorsLight*, on_light_event_cb, void*)
{
}

// Ambient Light Sensor Event
uint64_t uas_light_event_get_timestamp(UASLightEvent*)
{
}

float uas_light_event_get_light(UASLightEvent*)
{
}
