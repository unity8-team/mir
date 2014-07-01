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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_LOCATION_POSITION_UPDATE_H_
#define UBUNTU_APPLICATION_LOCATION_POSITION_UPDATE_H_

#include <ubuntu/visibility.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * \brief Opaque type encapsulating a location update.
     * \ingroup location_service
     */
    typedef struct UbuntuApplicationLocationPositionUpdate UALocationPositionUpdate;

    /**
     * \brief Increments the reference count of the position update instance.
     * \ingroup location_service
     * \param[in] update The position update instance to increment the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_position_update_ref(
        UALocationPositionUpdate *update);

    /**
     * \brief Decrements the reference count of the position update instance.
     * \ingroup location_service
     * \param[in] update The position update instance to decrement the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_position_update_unref(
        UALocationPositionUpdate *update);

    /**
     * \brief Queries the timestamp of the position update.
     * \ingroup location_service
     * \returns The timestamp of the position update in [µs].
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC uint64_t
    ua_location_position_update_get_timestamp(
        UALocationPositionUpdate *update);

    /**
     * \brief Queries the latitude contained in the position update.
     * \ingroup location_service
     * \returns The latitude of the position update in [°].
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC double
    ua_location_position_update_get_latitude_in_degree(
        UALocationPositionUpdate *update);

    /**
     * \brief Queries the longitude contained in the position update.
     * \ingroup location_service
     * \returns The longitude of the position update in [°].
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC double
    ua_location_position_update_get_longitude_in_degree(
        UALocationPositionUpdate *update);

    /**
     * \brief Checks if the position update contains an altitude.
     * \ingroup location_service
     * \returns TRUE if the update contains an altitude, else FALSE.
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC bool
    ua_location_position_update_has_altitude(
        UALocationPositionUpdate *update);

    /**
     * \brief Queries the altitude contained in the position update.
     * \ingroup location_service
     * \returns The altitude of the position update in [m].
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC double
    ua_location_position_update_get_altitude_in_meter(
        UALocationPositionUpdate *update);

    /**
     * \brief Checks if the position update contains a horizontal accuracy estimate.
     * \ingroup location_service
     * \returns TRUE if the update contains a horizontal accuracy estimate, else FALSE.
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC bool
    ua_location_position_update_has_horizontal_accuracy(
        UALocationPositionUpdate *update);

    /**
     * \brief Queries the horizontal accuracy contained in the position update.
     * \ingroup location_service
     * \returns The horizontal accuracy of the position update in [m].
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC double
    ua_location_position_update_get_horizontal_accuracy_in_meter(
        UALocationPositionUpdate *update);

    /**
     * \brief Checks if the position update contains a vertical accuracy estimate.
     * \ingroup location_service
     * \returns TRUE if the update contains a vertical accuracy estimate, else FALSE.
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC bool
    ua_location_position_update_has_vertical_accuracy(
        UALocationPositionUpdate *update);

    /**
     * \brief Queries the vertical accuracy contained in the position update.
     * \ingroup location_service
     * \returns The vertical accuracy of the position update in [m].
     * \param[in] update The position update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC double
    ua_location_position_update_get_vertical_accuracy_in_meter(
        UALocationPositionUpdate *update);

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_LOCATION_POSITION_UPDATE_H_
