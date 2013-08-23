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
 *              Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_ID_H_
#define UBUNTU_APPLICATION_ID_H_

#include <ubuntu/visibility.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \brief An opaque type describing an application ID.
     * \ingroup application_support
     */
    typedef void UApplicationId;

    /**
     * \brief Creates a new application ID from an existing string.
     * \ingroup application_support
     * \param[in] string The string containing the application ID.
     * \param[in] size The size of the string.
     */
    UBUNTU_DLL_PUBLIC UApplicationId*
    u_application_id_new_from_stringn(
        const char *string,
        size_t size);

    /**
     * \brief Destroy the supplied application ID instance.
     * \ingroup application_support
     * \param[in] id The instance to be destroyed.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_id_destroy(UApplicationId *id);

    /**
     * \brief Compares two application ID instances.
     * \ingroup application_support
     * \returns -1 iff lhs < rhs, 0 iff lhs == rhs, 1 iff lhs > rhs.
     * \param[in] lhs The left-hand-side id to be compared.
     * \param[in] rhs The right-hand-side id to be compared.
     */
    UBUNTU_DLL_PUBLIC int
    u_application_id_compare(
        UApplicationId *lhs,
        UApplicationId *rhs);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_ID_H_ */
