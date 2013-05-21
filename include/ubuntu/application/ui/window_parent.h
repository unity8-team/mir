#ifndef UBUNTU_APPLICATION_UI_WINDOW_PARENT_H_
#define UBUNTU_APPLICATION_UI_WINDOW_PARENT_H_

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

#endif /* UBUNTU_APPLICATION_UI_WINDOW_PARENT_H_ */