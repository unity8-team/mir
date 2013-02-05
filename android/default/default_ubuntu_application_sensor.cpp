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

#include <ubuntu/application/sensors/sensor.h>
#include <ubuntu/application/sensors/sensor_listener.h>
#include <ubuntu/application/sensors/sensor_service.h>
#include <ubuntu/application/sensors/sensor_type.h>

#include <cassert>
#include <cstdio>

namespace
{
template<ubuntu::application::sensors::SensorType sensor_type>
struct SensorListener : public ubuntu::application::sensors::SensorListener
{
    SensorListener() : observer(NULL)
    {
    }

    void on_new_reading(const ubuntu::application::sensors::SensorReading::Ptr& reading)
    {
        if(!observer)
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

void ubuntu_sensor_install_observer(ubuntu_sensor_observer* observer)
{
    assert(observer);
    if (observer->on_new_accelerometer_reading_cb && accelerometer == NULL)
    {
        accelerometer =
            ubuntu::application::sensors::SensorService::sensor_for_type(
                ubuntu::application::sensors::sensor_type_accelerometer);

        SensorListener<ubuntu::application::sensors::sensor_type_accelerometer>* sl 
                       = new SensorListener<ubuntu::application::sensors::sensor_type_accelerometer>();
        sl->observer = observer;

        accelerometer_listener = sl;
        accelerometer->register_listener(accelerometer_listener);
        accelerometer->enable();
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
