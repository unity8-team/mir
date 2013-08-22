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

#ifndef UBUNTU_APPLICATION_DESCRIPTION_H_
#define UBUNTU_APPLICATION_DESCRIPTION_H_

#include <ubuntu/visibility.h>

#include <ubuntu/application/id.h>
#include <ubuntu/application/lifecycle_delegate.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \brief Encapsulates properties of an application instance.
     * \ingroup application_support
     */
    typedef void UApplicationDescription;

    /**
     * \brief Creates a new instance of application description.
     * \ingroup application_support
     * \returns A new application description instance or NULL if not enough memory is available.
     */
    UBUNTU_DLL_PUBLIC UApplicationDescription*
    u_application_description_new();

    /**
     * \brief Destroys an instance of application description and releases all resources.
     * \ingroup application_support
     * \param[in] desc The instance to be destroyed.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_description_destroy(
        UApplicationDescription *desc);

    /**
     * \brief Sets the application id contained in the description instance.
     * \ingroup application_support
     * \param[in] desc The application description instance, must not be NULL.
     * \param[in] id The new application id, must not be NULL.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_description_set_application_id(
        UApplicationDescription *desc,
        UApplicationId *id);

    /**
     * \brief Queries the application id contained in the description instance.
     * \ingroup application_support
     * \returns The app id contained in the instance.
     * \param[in] desc The application description instance, must not be NULL.
     */
    UBUNTU_DLL_PUBLIC UApplicationId*
    u_application_description_get_application_id(
        UApplicationDescription *desc);

    /**
     * \brief Sets the application lifecycle delegate
     * \ingroup application_support
     * \param[in] desc The application description instance, must not be NULL.
     * \param[in] lifecycle_delegate The new lifecycle delegate, must not be NULL.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_description_set_application_lifecycle_delegate(
        UApplicationDescription *desc,
        UApplicationLifecycleDelegate *lifecycle_delegate);

    /**
     * \brief Queries the application lifecycle delegate
     * \ingroup application_support
     * \returns The application lifecycle delegate contained in the description instance.
     * \param[in] desc The application description instance, must not be NULL.
     */
    UBUNTU_DLL_PUBLIC UApplicationLifecycleDelegate*
    u_application_description_get_application_lifecycle_delegate(
        UApplicationDescription *desc);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_DESCRIPTION_H_ */
