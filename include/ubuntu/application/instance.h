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

#ifndef UBUNTU_APPLICATION_INSTANCE_H_
#define UBUNTU_APPLICATION_INSTANCE_H_

#include <ubuntu/visibility.h>

#include <ubuntu/application/description.h>
#include <ubuntu/application/options.h>

#ifdef __cplusplus
extern "C" {
#endif

    /** \defgroup application_support Functions and types to support application development. */

    /**
     * \brief Opaque type describing an application instance.
     * \ingroup application_support
     * An application instance encapsulates the event loop of an app.
     */
    typedef void UApplicationInstance;

    /**
     * \brief Creates a new application instance with a reference count of 1.
     * \ingroup application_support
     * \returns A new application instance or NULL in case of low-memory.
     * \param[in] desc A description object, must not be NULL.
     * \param[in] options An options object, must not be NULL.
     */
    UBUNTU_DLL_PUBLIC UApplicationInstance*
    u_application_instance_new_from_description_with_options(
        UApplicationDescription *desc,
        UApplicationOptions *options);

    /**
     * \brief Increments the reference count of an application instance.
     * \ingroup application_support
     * \param[in] instance The instance to increment the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_instance_ref(
        UApplicationInstance *instance);

    /**
     * \brief Decrements the reference count of an application instance and releases all resources held by the object if the reference count reaches 0.
     * \ingroup application_support
     * \param[in] instance The instance to decrement the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_instance_unref(
        UApplicationInstance *instance);

    /**
     * \brief Destroys the application instance and releases all its resources.
     * \ingroup application_support
     * \param[in] instance The instance to be destroyed.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_instance_destroy(
        UApplicationInstance *instance);

    /**
     * \brief Executes the event loop of the application instance
     * \ingroup application_support
     * \param[in] instance The instance to be executed.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_instance_run(
        UApplicationInstance *instance);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_INSTANCE_H_ */
