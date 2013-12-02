/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by: Martin Pitt <martin.pitti@ubuntu.com>
 */

#include <ubuntu/application/sensors/ubuntu_application_sensors.h>
#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

#include <stddef.h>
#include <iostream>

using namespace std;

class TestSensor
{
  public:
    TestSensor(ubuntu_sensor_type _type, float _max_value, float _resolution) :
        type(_type),
        enabled(false),
        resolution(_resolution),
        min_delay(0),
        min_value(0.0),
        max_value(_max_value),
        on_event_cb(NULL),
        event_cb_context(NULL),
        x(0.0),
        y(0.0),
        z(0.0),
        distance((UASProximityDistance) 0),  // LP#1256969
        timestamp(0)
    {}

  public:  /* consenting adults -- this is only internal API */
    ubuntu_sensor_type type;
    bool enabled;
    float resolution;
    uint32_t min_delay;
    float min_value, max_value;
    void (*on_event_cb)(void*, void*);
    void* event_cb_context;

    /* current value; note that we do not track separate Event objects/pointers
     * at all, and just always deliver the current value */
    float x, y, z;
    UASProximityDistance distance;
    uint64_t timestamp;
};

/***************************************
 *
 * Acceleration API
 *
 ***************************************/

UASensorsAccelerometer* ua_sensors_accelerometer_new()
{
    return new TestSensor(ubuntu_sensor_type_accelerometer, 1000.0, 0.1);
}

UStatus ua_sensors_accelerometer_enable(UASensorsAccelerometer* s)
{
    static_cast<TestSensor*>(s)->enabled = true;
    return (UStatus) 0;
}

UStatus ua_sensors_accelerometer_disable(UASensorsAccelerometer* s)
{
    static_cast<TestSensor*>(s)->enabled = false;
    return (UStatus) 0;
}

uint32_t ua_sensors_accelerometer_get_min_delay(UASensorsAccelerometer* s)
{
    return static_cast<TestSensor*>(s)->min_delay;
}

float ua_sensors_accelerometer_get_min_value(UASensorsAccelerometer* s)
{
    return static_cast<TestSensor*>(s)->min_value;
}

float ua_sensors_accelerometer_get_max_value(UASensorsAccelerometer* s)
{
    return static_cast<TestSensor*>(s)->max_value;
}

float ua_sensors_accelerometer_get_resolution(UASensorsAccelerometer* s)
{
    return static_cast<TestSensor*>(s)->resolution;
}

void ua_sensors_accelerometer_set_reading_cb(UASensorsAccelerometer* s, on_accelerometer_event_cb cb, void* ctx)
{
    TestSensor* sensor = static_cast<TestSensor*>(s);
    sensor->on_event_cb = cb;
    sensor->event_cb_context = ctx;
}

uint64_t uas_accelerometer_event_get_timestamp(UASAccelerometerEvent* e)
{
    return static_cast<TestSensor*>(e)->timestamp;
}

float uas_accelerometer_event_get_acceleration_x(UASAccelerometerEvent* e)
{
    return static_cast<TestSensor*>(e)->x;
}

float uas_accelerometer_event_get_acceleration_y(UASAccelerometerEvent* e)
{
    return static_cast<TestSensor*>(e)->y;
}

float uas_accelerometer_event_get_acceleration_z(UASAccelerometerEvent* e)
{
    return static_cast<TestSensor*>(e)->z;
}

/***************************************
 *
 * Proximity API
 *
 ***************************************/

UASensorsProximity* ua_sensors_proximity_new()
{
    return new TestSensor(ubuntu_sensor_type_proximity, 0.0, 0.0);
}

UStatus ua_sensors_proximity_enable(UASensorsProximity* s)
{
    static_cast<TestSensor*>(s)->enabled = true;
    return (UStatus) 0;
}

UStatus ua_sensors_proximity_disable(UASensorsProximity* s)
{
    static_cast<TestSensor*>(s)->enabled = false;
    return (UStatus) 0;
}

uint32_t ua_sensors_proximity_get_min_delay(UASensorsProximity* s)
{
    return static_cast<TestSensor*>(s)->min_delay;
}

// the next three function make no sense in the API, just return zero
float ua_sensors_proximity_get_min_value(UASensorsProximity*)
{
    return 0.0;
}

float ua_sensors_proximity_get_max_value(UASensorsProximity*)
{
    return 0.0;
}

float ua_sensors_proximity_get_resolution(UASensorsProximity*)
{
    return 0.0;
}

void ua_sensors_proximity_set_reading_cb(UASensorsProximity* s, on_proximity_event_cb cb, void* ctx)
{
    TestSensor* sensor = static_cast<TestSensor*>(s);
    sensor->on_event_cb = cb;
    sensor->event_cb_context = ctx;
}

uint64_t uas_proximity_event_get_timestamp(UASProximityEvent* e)
{
    return static_cast<TestSensor*>(e)->timestamp;
}

UASProximityDistance uas_proximity_event_get_distance(UASProximityEvent* e)
{
    return static_cast<TestSensor*>(e)->distance;
}


/***************************************
 *
 * Light API
 *
 ***************************************/

UASensorsLight* ua_sensors_light_new()
{
    return new TestSensor(ubuntu_sensor_type_light, 1.0, 0.1);
}

UStatus ua_sensors_light_enable(UASensorsLight* s)
{
    static_cast<TestSensor*>(s)->enabled = true;
    return (UStatus) 0;
}

UStatus ua_sensors_light_disable(UASensorsLight* s)
{
    static_cast<TestSensor*>(s)->enabled = false;
    return (UStatus) 0;
}

uint32_t ua_sensors_light_get_min_delay(UASensorsLight* s)
{
    return static_cast<TestSensor*>(s)->min_delay;
}

float ua_sensors_light_get_min_value(UASensorsLight* s)
{
    return static_cast<TestSensor*>(s)->min_value;
}

float ua_sensors_light_get_max_value(UASensorsLight* s)
{
    return static_cast<TestSensor*>(s)->max_value;
}

float ua_sensors_light_get_resolution(UASensorsLight* s)
{
    return static_cast<TestSensor*>(s)->resolution;
}

void ua_sensors_light_set_reading_cb(UASensorsLight* s, on_light_event_cb cb, void* ctx)
{
    TestSensor* sensor = static_cast<TestSensor*>(s);
    sensor->on_event_cb = cb;
    sensor->event_cb_context = ctx;
}

uint64_t uas_light_event_get_timestamp(UASLightEvent* e)
{
    return static_cast<TestSensor*>(e)->timestamp;
}

float uas_light_event_get_light(UASLightEvent* e)
{
    return static_cast<TestSensor*>(e)->x;
}
