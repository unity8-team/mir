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

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UApplicationId;
    
    UApplicationId*
    u_application_id_new_from_stringn(
    	const char *string, 
    	size_t size);
    
    void
    u_application_id_destroy(UApplicationId *id);
    
    int
    u_application_id_compare(
    	UApplicationId *lhs,
    	UApplicationId *rhs);
       
#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_ID_H_ */
