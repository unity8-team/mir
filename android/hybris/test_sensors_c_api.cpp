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

#include <ubuntu/application/sensors/ubuntu_application_sensors.h>

#include <stdio.h>
#include <string.h>

void cb_on_new_accelerometer_reading(ubuntu_sensor_accelerometer_reading* reading, void*)
{
    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %d\n", reading->timestamp);
    printf("\tx: %f\n", reading->acceleration_x);
    printf("\ty: %f\n", reading->acceleration_y);
    printf("\tz: %f\n", reading->acceleration_z);
}

void cb_on_new_proximity_reading(ubuntu_sensor_proximity_reading* reading, void*)
{
    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %d\n", reading->timestamp);
    printf("\tdistance: %f\n", reading->distance);
}

void cb_on_new_light_reading(ubuntu_sensor_ambient_light_reading* reading, void*)
{
    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %d\n", reading->timestamp);
    printf("\tdistance: %f\n", reading->light);
}

int main(int argc, char** argv)
{
    ubuntu_sensor_observer observer;
    memset(&observer, 0, sizeof(observer));

    observer.on_new_accelerometer_reading_cb = cb_on_new_accelerometer_reading;
    observer.on_new_proximity_reading_cb = cb_on_new_proximity_reading;
    observer.on_new_ambient_light_reading_cb = cb_on_new_light_reading;

    ubuntu_sensor_install_observer(&observer);

    while(true)
    {
    }
}
