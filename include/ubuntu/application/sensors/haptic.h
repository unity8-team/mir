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

#ifndef UBUNTU_APPLICATION_SENSORS_HAPTIC_H_
#define UBUNTU_APPLICATION_SENSORS_HAPTIC_H_

#include <ubuntu/status.h>
#include <ubuntu/visibility.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \brief Opaque type that models the haptics device.
     * \ingroup sensor_access
     */
    typedef void UASensorsHaptic;

    /**
     * \brief Create a new object for accessing the haptics device.
     * \ingroup sensor_access
     * \returns A new instance or NULL in case of errors.
     */
    UBUNTU_DLL_PUBLIC UASensorsHaptic*
    ua_sensors_haptic_new();

    /**
     * \brief Enables the supplied haptics device.
     * \ingroup sensor_access
     * \returns U_STATUS_SUCCESS if successful or U_STATUS_ERROR if an error occured.
     * \param[in] sensor The sensor instance to be enabled.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_sensors_haptic_enable(
        UASensorsHaptic* sensor);

    /**
     * \brief Disables the supplied haptics device.
     * \ingroup sensor_access
     * \returns U_STATUS_SUCCESS if successful or U_STATUS_ERROR if an error occured.
     * \param[in] sensor The sensor instance to be disabled.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_sensors_haptic_disable(
        UASensorsHaptic* sensor);

    /**
     * \brief Run the vibrator for a fixed duration.
     * \ingroup sensor_access
     * \returns U_STATUS_SUCCESS if pushed correctly, U_STATUS_ERROR if the pattern limit is invalid or the actuator cannot be activated.
     * \param[in] sensor Haptic device to activate.
     * \param[in] duration How long should the vibrator stay on.
     */
     UBUNTU_DLL_PUBLIC UStatus
     ua_sensors_haptic_vibrate_once(
        UASensorsHaptic* sensor,
        uint32_t duration);
        
    #define MAX_PATTERN_SIZE 6

    /**
     * \brief Run the vibrator with a pattern and repeat a precise number of times.
     * \ingroup sensor_access
     * \returns U_STATUS_SUCCESS if pushed correctly, U_STATUS_ERROR if the pattern limit is invalid or the actuator cannot be activated.
     * \param[in] sensor Haptic device to activate.
     * \param[in] pattern An array of uint32_t durations for which to keep the vibrator on or off. The first value indicates how long to keep the vibrator on for, the second value how long to keep it off for, and so on until the end of the array.
     * \param[in] repeat How many times to repeat the whole pattern for.
     */
    
     UBUNTU_DLL_PUBLIC UStatus
     ua_sensors_haptic_vibrate_with_pattern(
        UASensorsHaptic* sensor,
        uint32_t pattern[MAX_PATTERN_SIZE],
        uint32_t repeat);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_SENSORS_HAPTIC_H_ */
