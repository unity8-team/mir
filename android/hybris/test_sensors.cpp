/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#include <ubuntu/application/sensors/sensor_service.h>

#include <ubuntu/application/sensors/sensor.h>
#include <ubuntu/application/sensors/sensor_listener.h>
#include <ubuntu/application/sensors/sensor_reading.h>

#include <cstdio>

#include <unistd.h>

namespace uas = ubuntu::application::sensors;

struct SensorListener : public uas::SensorListener
{
    SensorListener(uas::SensorType sensor_type) : sensor_type(sensor_type)
    {
    }

    void on_new_reading(const uas::SensorReading::Ptr& reading)
    {
        on_new_reading(sensor_type, reading);
    }

    void on_new_reading(
        uas::SensorType sensor_type,
        const uas::SensorReading::Ptr& reading)
    {
        switch (sensor_type)
        {
        case uas::sensor_type_accelerometer:
            printf("New reading for accelerometer: [%f, %f, %f] \n",
                   reading->acceleration[0],
                   reading->acceleration[1],
                   reading->acceleration[2]);
            break;
        case uas::sensor_type_magnetic_field:
            printf("New reading for magnetic field: [%f, %f, %f] \n",
                   reading->magnetic[0],
                   reading->magnetic[1],
                   reading->magnetic[2]);
            break;
        case uas::sensor_type_gyroscope:
            printf("New reading for gyro: [%f, %f, %f] \n",
                   reading->vector[0],
                   reading->vector[1],
                   reading->vector[2]);
            break;
        case uas::sensor_type_light:
            printf("New reading for light sensor: [%f] \n",
                   reading->light);
            break;
        case uas::sensor_type_proximity:
            printf("New reading for proximity sensor: [%f] \n",
                   reading->distance);
            break;
        case uas::sensor_type_orientation:
            printf("New reading for orientation sensor: [%f, %f, %f] \n",
                   reading->vector[0],
                   reading->vector[1],
                   reading->vector[2]);
            break;
        case uas::sensor_type_linear_acceleration:
            printf("New reading for linear acceleration: [%f, %f, %f] \n",
                   reading->vector[0],
                   reading->vector[1],
                   reading->vector[2]);
            break;
        case uas::sensor_type_rotation_vector:
            printf("New reading for rotation vector: [%f, %f, %f] \n",
                   reading->vector[0],
                   reading->vector[1],
                   reading->vector[2]);
            break;
        }
    }
    uas::SensorType sensor_type;
};

int main(int argc, char** argv)
{
    ubuntu::application::sensors::Sensor::Ptr sensors[ubuntu::application::sensors::undefined_sensor_type];

    for(int i = ubuntu::application::sensors::first_defined_sensor_type;
            i < ubuntu::application::sensors::undefined_sensor_type;
            i++)
    {
        sensors[i] = uas::SensorService::sensor_for_type(
                         static_cast<ubuntu::application::sensors::SensorType>(i));

        if (sensors[i] == NULL)
            continue;

        printf("Sensor: %s by %s (power consumption: %f)\n",
               sensors[i]->name(),
               sensors[i]->vendor(),
               sensors[i]->power_consumption());
        sensors[i]->register_listener(
            uas::SensorListener::Ptr(
                new SensorListener(
                    static_cast<ubuntu::application::sensors::SensorType>(i))));
        sensors[i]->enable();
    }

    while (true)
    {
        sleep(1);
    }
}
