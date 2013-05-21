#ifndef UBUNTU_APPLICATION_UI_WINDOW_H_
#define UBUNTU_APPLICATION_UI_WINDOW_H_

#include <EGL/egl.h>

#include <ubuntu/status.h>
#include <ubuntu/application/instance.h>
#include <ubuntu/application/ui/window_properties.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UAUiWindow;
    typedef int32_t UAUiWindowId;
    
    UAUiWindow*
    ua_ui_window_new_for_application_with_properties(
    	UApplicationInstance *instance,
    	UAUiWindowProperties *properties);
    
    void
    ua_ui_window_destroy(
    	UAUiWindow *window);
    
    UAUiWindowId
    ua_ui_window_get_id(
    	UAUiWindow *window);
    
    UStatus
    ua_ui_window_move(
        UAUiWindow *window,
        uint32_t new_x,
        uint32_t new_y);

    UStatus
    ua_ui_window_resize(
    	UAUiWindow *window,
    	uint32_t new_width,
    	uint32_t new_height);
    
    UStatus
    ua_ui_window_hide(
    	UAUiWindow *window);
    
    UStatus
    ua_ui_window_show(
    	UAUiWindow *window);

    void
    ua_ui_window_request_fullscreen(
        UAUiWindow *window);

    EGLNativeWindowType
    ua_ui_window_get_native_type(
        UAUiWindow *window);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_WINDOW_H_ */
