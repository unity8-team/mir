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

#ifndef UBUNTU_APPLICATION_SENSORS_PROXIMITY_EVENT_H_
#define UBUNTU_APPLICATION_SENSORS_PROXIMITY_EVENT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum {
        U_PROXIMITY_NEAR = 1,
        U_PROXIMITY_FAR = 2
    } UASProximityDistance;

    typedef void UASProximityEvent;

    uint64_t
    uas_proximity_event_get_timestamp(
        UASProximityEvent* event);
   
    UASProximityDistance
    uas_proximity_event_get_distance(
        UASProximityEvent* event);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_SENSORS_PROXIMITY_EVENT_H_ */
