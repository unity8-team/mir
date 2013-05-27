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

#include "id.h"
#include "lifecycle_delegate.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UApplicationDescription;
    
    UApplicationDescription*
    u_application_description_new();
    
    void
    u_application_description_destroy(
    	UApplicationDescription *desc);
    
    void
    u_application_description_set_application_id(
    	UApplicationDescription *desc,
    	UApplicationId *id);
    
    UApplicationId* 
    u_application_description_get_application_id(
    	UApplicationDescription *desc);
    
    void
    u_application_description_set_application_lifecycle_delegate(
    	UApplicationDescription *desc,
    	UApplicationLifecycleDelegate *lifecycle_delegate);
    
    UApplicationLifecycleDelegate* 
    u_application_description_get_application_lifecycle_delegate(
    	UApplicationDescription *desc);
   
#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_DESCRIPTION_H_ */
