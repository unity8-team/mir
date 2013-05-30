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

#ifndef UBUNTU_APPLICATION_UI_WINDOW_PARENT_H_
#define UBUNTU_APPLICATION_UI_WINDOW_PARENT_H_

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum 
    {
    	U_WINDOW_PARENT_WINDOW,
    	U_WINDOW_PARENT_APPLICATION,
    	U_WINDOW_PARENT_SESSION,
    	U_WINDOW_PARENT_SYSTEM
    } UApplicationUiWindowParentType;
    
    typedef struct UApplicationUiWindowParent;
    
    UApplicationUiWindowParent*
    u_application_ui_window_parent_new_with_window(
    	UApplicationUiWindow *window);
    
    UApplicationUiWindowParent*
    u_application_ui_window_parent_new_with_application(
    	UApplicationInstance *instance);
    
    UApplicationUiWindowParent*
    u_application_ui_window_parent_new_for_session();
    
    UApplicationUiWindowParent*
    u_application_ui_window_parent_new_for_system();
    
    void
    u_application_ui_window_parent_destroy(
    	UApplicationUiWindowParent *parent);
    
    UApplicationUiWindowParentType
    u_application_ui_window_parent_get_type(
    	UApplicationUiWindowParent *parent);
    
    UApplicationUiWindow*
    u_application_ui_window_parent_get_parent_window(
    	UApplicationUiWindowParent *parent);

#ifdef __cplusplus
}
#endif
    
#endif /* UBUNTU_APPLICATION_UI_WINDOW_PARENT_H_ */
