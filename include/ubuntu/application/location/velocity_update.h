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

#ifndef UBUNTU_APPLICATION_LOCATION_VELOCITY_UPDATE_H_
#define UBUNTU_APPLICATION_LOCATION_VELOCITY_UPDATE_H_

#include <ubuntu/visibility.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * \brief Opaque type encapsulating a velocity update.
     * \ingroup location_service
     */
    typedef struct UbuntuApplicationLocationVelocityUpdate UALocationVelocityUpdate;

    /**
     * \brief Increments the reference count of the velocity update instance.
     * \ingroup location_service
     * \param[in] update The velocity update instance to increment the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_velocity_update_ref(
        UALocationVelocityUpdate *update);

    /**
     * \brief Decrements the reference count of the velocity update instance.
     * \ingroup location_service
     * \param[in] update The velocity update instance to decrement the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_velocity_update_unref(
        UALocationVelocityUpdate *update);

    /**
     * \brief Queries the timestamp of the velocity update.
     * \ingroup location_service
     * \returns The timestamp of the position update in [µs].
     * \param[in] update The velocity update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC uint64_t
    ua_location_velocity_update_get_timestamp(
        UALocationVelocityUpdate *update);

    /**
     * \brief Queries the velocity contained in the position update.
     * \ingroup location_service
     * \returns The velocity in the position update in [m/s].
     * \param[in] update The velocity update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC double
    ua_location_velocity_update_get_velocity_in_meters_per_second(
        UALocationVelocityUpdate *update);

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_LOCATION_VELOCITY_UPDATE_H_
