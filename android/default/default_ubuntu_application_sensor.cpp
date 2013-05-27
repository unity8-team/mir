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

#include <ubuntu/application/sensors/ubuntu_application_sensors.h>

#include <private/application/sensors/sensor.h>
#include <private/application/sensors/sensor_listener.h>
#include <private/application/sensors/sensor_service.h>
#include <private/application/sensors/sensor_type.h>

#include <cassert>
#include <cstdio>

namespace
{
enum sensor_value_t { MIN_DELAY, MIN_VALUE, MAX_VALUE, RESOLUTION };
template<ubuntu::application::sensors::SensorType sensor_type>
struct SensorListener : public ubuntu::application::sensors::SensorListener
{
    SensorListener() : observer(NULL)
    {
    }

    void on_new_reading(const ubuntu::application::sensors::SensorReading::Ptr& reading)
    {
        if (!observer)
            return;

        switch(sensor_type)
        {
            case ubuntu::application::sensors::sensor_type_accelerometer:
                {
                    if (!observer->on_new_accelerometer_reading_cb)
                        return;

                    static ubuntu_sensor_accelerometer_reading r;
                    r.timestamp = reading->timestamp;
                    r.acceleration_x = reading->acceleration[0];
                    r.acceleration_y = reading->acceleration[1];
                    r.acceleration_z = reading->acceleration[2];

                    observer->on_new_accelerometer_reading_cb(&r, observer->context);
                    break;
                }
            case ubuntu::application::sensors::sensor_type_proximity:
                {
                    if (!observer->on_new_proximity_reading_cb)
                        return;

                    static ubuntu_sensor_proximity_reading r;
                    r.timestamp = reading->timestamp;
                    r.distance = reading->distance;

                    observer->on_new_proximity_reading_cb(&r, observer->context);
                    break;
                }
            case ubuntu::application::sensors::sensor_type_light:
                {
                    if (!observer->on_new_ambient_light_reading_cb)
                        return;

                    static ubuntu_sensor_ambient_light_reading r;
                    r.timestamp = reading->timestamp;
                    r.light = reading->light;

                    observer->on_new_ambient_light_reading_cb(&r, observer->context);
                    break;
                }
        }
    }

    ubuntu_sensor_observer* observer;
};

ubuntu::application::sensors::Sensor::Ptr accelerometer;
ubuntu::application::sensors::Sensor::Ptr proximity;
ubuntu::application::sensors::Sensor::Ptr light;
ubuntu::application::sensors::SensorListener::Ptr accelerometer_listener;
ubuntu::application::sensors::SensorListener::Ptr proximity_listener;
ubuntu::application::sensors::SensorListener::Ptr light_listener;
}

void ubuntu_sensor_initialize_observer(ubuntu_sensor_observer* observer)
{
    if (observer == NULL)
        return;

    observer->on_new_proximity_reading_cb = NULL;
    observer->on_new_ambient_light_reading_cb = NULL;
    observer->on_new_accelerometer_reading_cb = NULL;
    observer->context = NULL;
}

static void install_accelerometer_observer(ubuntu_sensor_observer* observer)
{
    assert(observer != NULL);
    assert(accelerometer != NULL);

    SensorListener<ubuntu::application::sensors::sensor_type_accelerometer>* sl 
        = new SensorListener<ubuntu::application::sensors::sensor_type_accelerometer>();
    sl->observer = observer;

    accelerometer_listener = sl;
    accelerometer->register_listener(accelerometer_listener);
}

void ubuntu_sensor_install_observer(ubuntu_sensor_observer* observer)
{
    assert(observer);
    if (observer->on_new_accelerometer_reading_cb && accelerometer == NULL)
    {
        // Only create the accelerometer if this is the first observer installation
        accelerometer =
            ubuntu::application::sensors::SensorService::sensor_for_type(
                ubuntu::application::sensors::sensor_type_accelerometer);
    }

    if (observer->on_new_accelerometer_reading_cb)
    {
        // Now install the observer instance so that its callback gets called with
        // a new accelerometer reading
        install_accelerometer_observer(observer);
    }

    if (observer->on_new_proximity_reading_cb && proximity == NULL)
    {
        proximity =
            ubuntu::application::sensors::SensorService::sensor_for_type(
                ubuntu::application::sensors::sensor_type_proximity);

        SensorListener<ubuntu::application::sensors::sensor_type_proximity>* sl 
                = new SensorListener<ubuntu::application::sensors::sensor_type_proximity>();
        sl->observer = observer;

        proximity_listener = sl;
        proximity->register_listener(proximity_listener);
        proximity->enable();
    }

    if (observer->on_new_ambient_light_reading_cb && light == NULL)
    {
        light =
            ubuntu::application::sensors::SensorService::sensor_for_type(
                ubuntu::application::sensors::sensor_type_light);

        SensorListener<ubuntu::application::sensors::sensor_type_light>* sl 
                = new SensorListener<ubuntu::application::sensors::sensor_type_light>();
        sl->observer = observer;

        light_listener = sl;
        light->register_listener(light_listener);
        light->enable();
    }
}

void ubuntu_sensor_uninstall_observer(ubuntu_sensor_observer* observer)
{
    if (observer == NULL)
        return;
}

static void sensor_set_state(ubuntu_sensor_type sensor_type, bool enable)
{
    switch (sensor_type)
    {
        case ubuntu_sensor_type_accelerometer:
            if (accelerometer != NULL)
                (enable) ? accelerometer->enable() : accelerometer->disable();
            break;
        case ubuntu_sensor_type_magnetic_field:
            break;
        case ubuntu_sensor_type_gyroscope:
            break;
        case ubuntu_sensor_type_light:
            if (light != NULL)
                (enable) ? light->enable() : light->disable();
            break;
        case ubuntu_sensor_type_proximity:
            if (proximity != NULL)
                (enable) ? proximity->enable() : proximity->disable();
            break;
        case ubuntu_sensor_type_orientation:
            break;
        case ubuntu_sensor_type_linear_acceleration:
            break;
        case ubuntu_sensor_type_rotation_vector:
            break;
        default:
            return;
    }
}

void ubuntu_sensor_enable_sensor(ubuntu_sensor_type sensor_type)
{
    sensor_set_state(sensor_type, true);
}

void ubuntu_sensor_disable_sensor(ubuntu_sensor_type sensor_type)
{
    sensor_set_state(sensor_type, false);
}

static int32_t toHz(int32_t microseconds)
{
    return 1 / (microseconds / 1e6);
}

static float sensor_range_value(ubuntu_sensor_type sensor_type, sensor_value_t value_type, float default_return = 0)
{
    ubuntu::application::sensors::Sensor::Ptr sensor;

    switch (sensor_type)
    {
        case ubuntu_sensor_type_accelerometer:
            sensor = accelerometer;
            break;
        case ubuntu_sensor_type_magnetic_field:
            break;
        case ubuntu_sensor_type_gyroscope:
            break;
        case ubuntu_sensor_type_light:
            sensor = light;
            break;
        case ubuntu_sensor_type_proximity:
            sensor = proximity;
            break;
        case ubuntu_sensor_type_orientation:
            break;
        case ubuntu_sensor_type_linear_acceleration:
            break;
        case ubuntu_sensor_type_rotation_vector:
            break;
        default:
            break;
    }

    if (sensor != NULL)
    {
        switch (value_type)
        {
            case MIN_DELAY:
                return toHz(sensor->min_delay());
            case MIN_VALUE:
                return sensor->min_value();
            case MAX_VALUE:
                return sensor->max_value();
            case RESOLUTION:
                return sensor->resolution();
        }
    }

    return default_return;
}

int32_t ubuntu_sensor_get_sensor_min_delay(ubuntu_sensor_type sensor_type)
{
    return static_cast<int32_t>(sensor_range_value(sensor_type, MIN_DELAY, -1.0));
}

float ubuntu_sensor_get_sensor_min_value(ubuntu_sensor_type sensor_type)
{
    return sensor_range_value(sensor_type, MIN_VALUE);
}

float ubuntu_sensor_get_sensor_max_value(ubuntu_sensor_type sensor_type)
{
    return sensor_range_value(sensor_type, MAX_VALUE);
}

float ubuntu_sensor_get_sensor_resolution(ubuntu_sensor_type sensor_type)
{
    return sensor_range_value(sensor_type, RESOLUTION);
}
