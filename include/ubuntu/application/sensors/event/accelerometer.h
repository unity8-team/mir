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

#ifndef UBUNTU_APPLICATION_SENSORS_ACCELEROMETER_EVENT_H_
#define UBUNTU_APPLICATION_SENSORS_ACCELEROMETER_EVENT_H_

#include <ubuntu/visibility.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \brief Opaque type describing an accelerometer reading.
     * \ingroup sensor_access
     */
    typedef void UASAccelerometerEvent;

    /**
     * \brief Query the timestamp of the sensor reading.
     * \ingroup sensor_access
     * \returns The timestamp of the sensor reading in [µs], timebase: monotonic clock.
     * \param[in] event The reading to be queried.
     */
    UBUNTU_DLL_PUBLIC uint64_t
    uas_accelerometer_event_get_timestamp(
        UASAccelerometerEvent* event);

    /**
     * \brief Query the acceleration in x-axis direction.
     * \ingroup sensor_access
     * \returns The acceleration in x-axis direction.
     * \param[in] event The reading to be queried.
     */
    UBUNTU_DLL_PUBLIC UStatus
    uas_accelerometer_event_get_acceleration_x(
        UASAccelerometerEvent* event,
        float* value);

    /**
     * \brief Query the acceleration in y-axis direction.
     * \ingroup sensor_access
     * \returns The acceleration in y-axis direction.
     * \param[in] event The reading to be queried.
     */
    UBUNTU_DLL_PUBLIC UStatus
    uas_accelerometer_event_get_acceleration_y(
        UASAccelerometerEvent* event,
        float* value);

    /**
     * \brief Query the acceleration in z-axis direction.
     * \ingroup sensor_access
     * \returns The acceleration in z-axis direction.
     * \param[in] event The reading to be queried.
     */
    UBUNTU_DLL_PUBLIC UStatus
    uas_accelerometer_event_get_acceleration_z(
        UASAccelerometerEvent* event,
        float* value);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_SENSORS_ACCELEROMETER_EVENT_H_ */
