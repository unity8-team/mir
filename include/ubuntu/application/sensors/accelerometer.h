/*
 * Copyright © 2013 Canonical Ltd.
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

#ifndef UBUNTU_APPLICATION_SENSORS_ACCELEROMETER_H_
#define UBUNTU_APPLICATION_SENSORS_ACCELEROMETER_H_

#include <ubuntu/status.h>
#include <ubuntu/application/sensors/event/accelerometer.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UASensorsAccelerometer;
    typedef void (*on_accelerometer_event_cb)(UASAccelerometerEvent* event,
                                              void* context);

    UASensorsAccelerometer*
    ua_sensors_accelerometer_new(); 

    UStatus
    ua_sensors_accelerometer_enable(
        UASensorsAccelerometer* sensor);

    UStatus
    ua_sensors_accelerometer_disable(
        UASensorsAccelerometer* sensor);

    uint32_t
    ua_sensors_accelerometer_get_min_delay(
        UASensorsAccelerometer* sensor);
    
    float
    ua_sensors_accelerometer_get_min_value(
        UASensorsAccelerometer* sensor);
  
    float
    ua_sensors_accelerometer_get_max_value(
        UASensorsAccelerometer* sensor);
 
    float
    ua_sensors_accelerometer_get_resolution(
        UASensorsAccelerometer* sensor);
    void
    ua_sensors_accelerometer_set_reading_cb(
        UASensorsAccelerometer* sensor,
        on_accelerometer_event_cb cb,
        void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_SENSORS_ACCELEROMETER_H_ */
