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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

void on_new_accelerometer_event(UASAccelerometerEvent* event, void* context)
{
    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %llu\n", uas_accelerometer_event_get_timestamp(event));
    printf("\tx: %f\n", uas_accelerometer_event_get_acceleration_x(event));
    printf("\ty: %f\n", uas_accelerometer_event_get_acceleration_y(event));
    printf("\tz: %f\n", uas_accelerometer_event_get_acceleration_z(event));
}

void on_new_proximity_event(UASProximityEvent* event, void* context)
{
    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %llu\n", uas_proximity_event_get_timestamp(event));

    switch (uas_proximity_event_get_distance(event))
    {
        case U_PROXIMITY_NEAR:
        {   
            printf("\tdistance: NEAR\n");
            break;
        }
        case U_PROXIMITY_FAR:
        {
            printf("\tdistance: FAR\n");
            break;
        }
    }
}

void on_new_light_event(UASLightEvent* event, void* context)
{
    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %llu\n", uas_light_event_get_timestamp(event));
    printf("\tlight: %f\n", uas_light_event_get_light(event));
}

int main(int argc, char** argv)
{
    UASensorsAccelerometer* accelerometer = ua_sensors_accelerometer_new();
    UASensorsProximity* proximity = ua_sensors_proximity_new();
    UASensorsLight* ambientlight = ua_sensors_light_new();

    ua_sensors_accelerometer_set_reading_cb(accelerometer,
                                            on_new_accelerometer_event,
                                            NULL);

    ua_sensors_proximity_set_reading_cb(proximity,
                                        on_new_proximity_event,
                                        NULL);
  
    ua_sensors_light_set_reading_cb(ambientlight,
                                    on_new_light_event,
                                    NULL);

    ua_sensors_accelerometer_enable(accelerometer);
    ua_sensors_proximity_enable(proximity);
    ua_sensors_light_enable(ambientlight);

    while(true)
    {
    }
}
