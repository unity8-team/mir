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

#ifndef UBUNTU_APPLICATION_UI_WINDOW_H_
#define UBUNTU_APPLICATION_UI_WINDOW_H_

#include <ubuntu/visibility.h>

#include <ubuntu/status.h>
#include <ubuntu/application/instance.h>
#include <ubuntu/application/ui/window_properties.h>

#include <EGL/egl.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UAUiWindow;
    typedef int32_t UAUiWindowId;

    /*
        Ownership of properties is given to the returned UAUiWindow
    */
    UBUNTU_DLL_PUBLIC UAUiWindow*
    ua_ui_window_new_for_application_with_properties(
    	UApplicationInstance *instance,
    	UAUiWindowProperties *properties);
    
    UBUNTU_DLL_PUBLIC void
    ua_ui_window_destroy(
    	UAUiWindow *window);
    
    UBUNTU_DLL_PUBLIC UAUiWindowId
    ua_ui_window_get_id(
    	UAUiWindow *window);
    
    UBUNTU_DLL_PUBLIC void
    ua_ui_window_get_size(
        UAUiWindow *window,
        uint32_t *width,
        uint32_t *height);

    UBUNTU_DLL_PUBLIC UStatus
    ua_ui_window_move(
        UAUiWindow *window,
        uint32_t new_x,
        uint32_t new_y);

    UBUNTU_DLL_PUBLIC UStatus
    ua_ui_window_resize(
    	UAUiWindow *window,
    	uint32_t new_width,
    	uint32_t new_height);
    
    UBUNTU_DLL_PUBLIC UStatus
    ua_ui_window_hide(
    	UAUiWindow *window);
    
    UBUNTU_DLL_PUBLIC UStatus
    ua_ui_window_show(
    	UAUiWindow *window);

    UBUNTU_DLL_PUBLIC void
    ua_ui_window_request_fullscreen(
        UAUiWindow *window);

    UBUNTU_DLL_PUBLIC EGLNativeWindowType
    ua_ui_window_get_native_type(
        UAUiWindow *window);

    UBUNTU_DLL_PUBLIC int
    ua_ui_window_is_focused(
        UAUiWindow *window);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_WINDOW_H_ */
