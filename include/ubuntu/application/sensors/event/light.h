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

#ifndef UBUNTU_APPLICATION_SENSORS_LIGHT_EVENT_H_
#define UBUNTU_APPLICATION_SENSORS_LIGHT_EVENT_H_

#include <ubuntu/visibility.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \brief Opaque type describing an ambient light sensor reading.
     * \ingroup sensor_access
     */
    typedef void UASLightEvent;

    /**
     * \brief Query the timestamp of the sensor reading.
     * \ingroup sensor_access
     * \returns The timestamp of the sensor reading in [µs], timebase: monotonic clock.
     * \param[in] event The reading to be queried.
     */
    UBUNTU_DLL_PUBLIC uint64_t
    uas_light_event_get_timestamp(
        UASLightEvent* event);

    /**
     * \brief Query the value measured by the ambient light sensor.
     * \ingroup sensor_access
     * \returns The ambient light level.
     * \param[in] event The reading to be queried.
     */
    UBUNTU_DLL_PUBLIC UStatus
    uas_light_event_get_light(
        UASLightEvent* event,
        float* value);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_SENSORS_LIGHT_EVENT_H_ */
