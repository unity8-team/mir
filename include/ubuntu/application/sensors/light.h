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

#ifndef UBUNTU_APPLICATION_SENSORS_LIGHT_H_
#define UBUNTU_APPLICATION_SENSORS_LIGHT_H_

#include <ubuntu/status.h>
#include <ubuntu/application/sensors/event/light.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UASensorsLight;
    typedef void (*on_light_event_cb)(UASLightEvent* event,
                                      void* context);

    UASensorsLight*
    ua_sensors_light_new(); 

    UStatus
    ua_sensors_light_enable(
        UASensorsLight* sensor);

    UStatus
    ua_sensors_light_disable(
        UASensorsLight* sensor);

    uint32_t
    ua_sensors_light_get_min_delay(
        UASensorsLight* sensor);
    
    float
    ua_sensors_light_get_min_value(
        UASensorsLight* sensor);
  
    float
    ua_sensors_light_get_max_value(
        UASensorsLight* sensor);
 
    float
    ua_sensors_light_get_resolution(
        UASensorsLight* sensor);
    void
    ua_sensors_light_set_reading_cb(
        UASensorsLight* sensor,
        on_light_event_cb cb,
        void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_SENSORS_LIGHT_H_ */
