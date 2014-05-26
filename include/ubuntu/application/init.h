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

#ifndef UBUNTU_APPLICATION_INIT_H_
#define UBUNTU_APPLICATION_INIT_H_

#include <ubuntu/visibility.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

    /**
     * \brief Queries the Module Version that is currently loaded.
     * \ingroup application_support
     * \param[out] major Major version.
     * \param[out] minor Minor version.
     * \param[out] patch Patch version.
     */
     UBUNTU_DLL_PUBLIC void
     u_application_module_version(
        uint32_t *major,
        uint32_t *minor,
        uint32_t *patch);

    /**
     * \brief Initializes the Ubuntu Application API backend with args.
     * \ingroup application_support
     * \param[in] args Opaque initialization arguments.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_init(
        void *args);

    /**
     * \brief Signal termination to the Application backend instance.
     * \ingroup application_support
     */
    UBUNTU_DLL_PUBLIC void
    u_application_finish();

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_INIT_H_ */
