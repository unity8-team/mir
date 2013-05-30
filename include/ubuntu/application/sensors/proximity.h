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

#ifndef UBUNTU_APPLICATION_SENSORS_PROXIMITY_H_
#define UBUNTU_APPLICATION_SENSORS_PROXIMITY_H_

#include <ubuntu/status.h>
#include <ubuntu/application/sensors/event/proximity.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UASensorsProximity;
    typedef void (*on_proximity_event_cb)(UASProximityEvent* event,
                                          void* context);

    UASensorsProximity*
    ua_sensors_proximity_new(); 

    UStatus
    ua_sensors_proximity_enable(
        UASensorsProximity* sensor);

    UStatus
    ua_sensors_proximity_disable(
        UASensorsProximity* sensor);

    uint32_t
    ua_sensors_proximity_get_min_delay(
        UASensorsProximity* sensor);
    
    float
    ua_sensors_proximity_get_min_value(
        UASensorsProximity* sensor);
  
    float
    ua_sensors_proximity_get_max_value(
        UASensorsProximity* sensor);
 
    float
    ua_sensors_proximity_get_resolution(
        UASensorsProximity* sensor);
    void
    ua_sensors_proximity_set_reading_cb(
        UASensorsProximity* sensor,
        on_proximity_event_cb cb,
        void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_SENSORS_PROXIMITY_H_ */
