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

#ifndef UBUNTU_APPLICATION_OPTIONS_H_
#define UBUNTU_APPLICATION_OPTIONS_H_

#include <ubuntu/visibility.h>

#include <ubuntu/application/operation_mode.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \brief Encapsulates options as passed to the application.
     * \ingroup application_support
     */
    typedef void UApplicationOptions;

    /**
     * \brief Parses options from the command line.
     * \ingroup application_support
     * \returns An options instance if parsing was successful, or 0 otheriwse.
     * \param[in] argc Number of arguments.
     * \param[in] argv Arguments.
     */
    UBUNTU_DLL_PUBLIC UApplicationOptions*
    u_application_options_new_from_cmd_line(
        int argc,
        char** argv);

    /**
     * \brief Destroys the options object and releases all resources.
     * \ingroup application_support
     * \param[in] options The object to be destroyed.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_options_destroy(
        UApplicationOptions *options);

    /**
     * \brief Queries the operation mode from the supplied options object.
     * \ingroup application_support
     * \returns The operation mode as stored in the options object.
     * \param[in] options The options object to be queried.
     */
    UBUNTU_DLL_PUBLIC UApplicationOperationMode
    u_application_options_get_operation_mode(
        UApplicationOptions *options);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_OPTIONS_H_ */
