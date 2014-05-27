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

#ifndef UBUNTU_APPLICATION_SENSORS_ORIENTATION_H_
#define UBUNTU_APPLICATION_SENSORS_ORIENTATION_H_

#include <ubuntu/status.h>
#include <ubuntu/visibility.h>

#include <ubuntu/application/sensors/event/orientation.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \brief Opaque type that models the orientation.
     * \ingroup sensor_access
     */
    typedef void UASensorsOrientation;

    /**
     * \brief Callback type used by applications to subscribe to orientation events.
     * \ingroup sensor_access
     */
    typedef void (*on_orientation_event_cb)(UASOrientationEvent* event,
                                              void* context);

    /**
     * \brief Create a new object for accessing the orientation sensor.
     * \ingroup sensor_access
     * \returns A new instance or NULL in case of errors.
     */
    UBUNTU_DLL_PUBLIC UASensorsOrientation*
    ua_sensors_orientation_new();

    /**
     * \brief Enables the supplied orientation sensor.
     * \ingroup sensor_access
     * \returns U_STATUS_SUCCESS if successful or U_STATUS_ERROR if an error occured.
     * \param[in] sensor The sensor instance to be enabled.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_sensors_orientation_enable(
        UASensorsOrientation* sensor);

    /**
     * \brief Disables the supplied orientation sensor.
     * \ingroup sensor_access
     * \returns U_STATUS_SUCCESS if successful or U_STATUS_ERROR if an error occured.
     * \param[in] sensor The sensor instance to be disabled.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_sensors_orientation_disable(
        UASensorsOrientation* sensor);

    /**
     * \brief Queries the minimum delay between two readings for the supplied sensor.
     * \ingroup sensor_access
     * \returns The minimum delay between two readings in [ms].
     * \param[in] sensor The sensor instance to be queried.
     */
    UBUNTU_DLL_PUBLIC uint32_t
    ua_sensors_orientation_get_min_delay(
        UASensorsOrientation* sensor);

    /**
     * \brief Queries the minimum value that can be reported by the sensor.
     * \ingroup sensor_access
     * \returns The minimum value that can be reported by the sensor.
     * \param[in] sensor The sensor instance to be queried.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_sensors_orientation_get_min_value(
        UASensorsOrientation* sensor,
        float* value);

    /**
     * \brief Queries the maximum value that can be reported by the sensor.
     * \ingroup sensor_access
     * \returns The maximum value that can be reported by the sensor.
     * \param[in] sensor The sensor instance to be queried.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_sensors_orientation_get_max_value(
        UASensorsOrientation* sensor,
        float* value);

    /**
     * \brief Queries the numeric resolution supported by the sensor
     * \ingroup sensor_access
     * \returns The numeric resolution supported by the sensor.
     * \param[in] sensor The sensor instance to be queried.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_sensors_orientation_get_resolution(
        UASensorsOrientation* sensor,
        float* value);

    /**
     * \brief Set the callback to be invoked whenever a new sensor reading is available.
     * \ingroup sensor_access
     * \param[in] sensor The sensor instance to associate the callback with.
     * \param[in] cb The callback to be invoked.
     * \param[in] ctx The context supplied to the callback invocation.
     */
    UBUNTU_DLL_PUBLIC void
    ua_sensors_orientation_set_reading_cb(
        UASensorsOrientation* sensor,
        on_orientation_event_cb cb,
        void *ctx);

    /**
     * \brief Set the sensor event delivery rate in nanoseconds..
     * \ingroup sensor_access
     * \returns U_STATUS_SUCCESS if successful or U_STATUS_ERROR if an error occured.
     * \param[in] sensor The sensor instance to be modified.
     * \param[in] rate The new event delivery rate.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_sensors_orientation_set_event_rate(
        UASensorsOrientation* sensor,
        uint32_t rate);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_SENSORS_ORIENTATION_H_ */
