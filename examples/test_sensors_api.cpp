/*
 * Copyright © 2014 Canonical Ltd.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>
#include <ubuntu/application/sensors/orientation.h>

void on_new_accelerometer_event(UASAccelerometerEvent* event, void* context)
{
    float x; uas_accelerometer_event_get_acceleration_x(event, &x);
    float y; uas_accelerometer_event_get_acceleration_y(event, &y);
    float z; uas_accelerometer_event_get_acceleration_z(event, &z);

    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %" PRIu64 "\n", uas_accelerometer_event_get_timestamp(event));
    printf("\tx: %f\n", x);
    printf("\ty: %f\n", y);
    printf("\tz: %f\n", z);
}

void on_new_proximity_event(UASProximityEvent* event, void* context)
{
    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %" PRIu64 "\n", uas_proximity_event_get_timestamp(event));

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
    float light = -1.f; uas_light_event_get_light(event, &light);

    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %" PRIu64 "\n", uas_light_event_get_timestamp(event));
    printf("\tlight: %f\n", light);
}

void on_new_orientation_event(UASOrientationEvent* event, void* context)
{
    float x; uas_orientation_event_get_azimuth(event, &x);
    float y; uas_orientation_event_get_pitch(event, &y);
    float z; uas_orientation_event_get_roll(event, &z);

    printf("%s \n", __PRETTY_FUNCTION__);
    printf("\ttime: %" PRIu64 "\n", uas_orientation_event_get_timestamp(event));
    printf("\tx: %f\n", x);
    printf("\ty: %f\n", y);
    printf("\tz: %f\n", z);
}

int main(int argc, char** argv)
{
    UASensorsAccelerometer* accelerometer = ua_sensors_accelerometer_new();
    UASensorsProximity* proximity = ua_sensors_proximity_new();
    UASensorsLight* ambientlight = ua_sensors_light_new();
    UASensorsOrientation* orientation = ua_sensors_orientation_new();

    ua_sensors_accelerometer_set_reading_cb(accelerometer,
                                            on_new_accelerometer_event,
                                            NULL);

    ua_sensors_proximity_set_reading_cb(proximity,
                                        on_new_proximity_event,
                                        NULL);
  
    ua_sensors_light_set_reading_cb(ambientlight,
                                    on_new_light_event,
                                    NULL);

    ua_sensors_orientation_set_reading_cb(orientation,
                                          on_new_orientation_event,
                                          NULL);

    ua_sensors_accelerometer_enable(accelerometer);
    ua_sensors_proximity_enable(proximity);
    ua_sensors_light_enable(ambientlight);
    ua_sensors_orientation_enable(orientation);

    while(true)
    {
    }
}
