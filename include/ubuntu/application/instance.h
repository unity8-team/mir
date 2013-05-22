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

#include "description.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UApplicationInstance;
    
    UApplicationInstance*
    u_application_instance_new_from_description_with_options(
    	UApplicationDescription *desc,
    	UApplicationOptions *options);
    
    void
    u_application_instance_ref(
    	UApplicationInstance *instance);
    
    void
    u_application_instance_unref(
    	UApplicationInstance *instance);
    
    void
    u_application_instance_destroy(
    	UApplicationInstance *instance);
    
    void
    u_application_instance_run(
    	UApplicationInstance *instance);

#ifdef __cplusplus
}
#endif
    
#endif /* UBUNTU_APPLICATION_INSTANCE_H_ */
