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

#ifndef UBUNTU_APPLICATION_UI_SESSION_H_
#define UBUNTU_APPLICATION_UI_SESSION_H_

#include <ubuntu/visibility.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UAUiSession;
    typedef void UAUiSessionProperties;
    
    typedef enum
    {
        U_USER_SESSION = 0,
        U_SYSTEM_SESSION = 1
    } UAUiSessionType;
    
    UBUNTU_DLL_PUBLIC UAUiSession*
    ua_ui_session_new_with_properties(
    	UAUiSessionProperties *properties);
    
    UBUNTU_DLL_PUBLIC UAUiSessionProperties*
    ua_ui_session_properties_new();
    
    UBUNTU_DLL_PUBLIC void
    ua_ui_session_properties_set_type(
        UAUiSessionProperties* properties,
        UAUiSessionType type);

    UBUNTU_DLL_PUBLIC void
    ua_ui_session_properties_set_remote_pid(
        UAUiSessionProperties* properties,
        uint32_t pid);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_SESSION_H_ */
