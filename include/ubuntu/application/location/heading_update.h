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

#ifndef UBUNTU_APPLICATION_LOCATION_HEADING_UPDATE_H_
#define UBUNTU_APPLICATION_LOCATION_HEADING_UPDATE_H_

#include <ubuntu/visibility.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * \brief Opaque type encapsulating a heading update.
     * \ingroup location_service
     */
    typedef struct UbuntuApplicationLocationHeadingUpdate UALocationHeadingUpdate;

    /**
     * \brief Increments the reference count of the heading update instance.
     * \ingroup location_service
     * \param[in] update The heading update instance to increment the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_heading_update_ref(
        UALocationHeadingUpdate *update);

    /**
     * \brief Decrements the reference count of the heading update instance.
     * \ingroup location_service
     * \param[in] update The heading update instance to decrement the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_heading_update_unref(
        UALocationHeadingUpdate *update);

    /**
     * \brief Queries the timestamp of the heading update.
     * \ingroup location_service
     * \returns The timestamp of the heading update in [µs].
     * \param[in] update The heading update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC uint64_t
    ua_location_heading_update_get_timestamp(
        UALocationHeadingUpdate *update);

    /**
     * \brief Queries the new heading from the update.
     * \ingroup location_service
     * \returns The new heading from the update in [°].
     * \param[in] update The heading update instance to be queried.
     */
    UBUNTU_DLL_PUBLIC double
    ua_location_heading_update_get_heading_in_degree(
        UALocationHeadingUpdate *update);

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_LOCATION_HEADING_UPDATE_H_
