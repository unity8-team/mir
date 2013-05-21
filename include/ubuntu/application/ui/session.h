#ifndef UBUNTU_APPLICATION_UI_SESSION_H_
#define UBUNTU_APPLICATION_UI_SESSION_H_

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
    
    UAUiSession*
    ua_ui_session_new_with_properties(
    	UAUiSessionProperties *properties);
    
    UAUiSessionProperties*
    ua_ui_session_properties_new();
    
    void
    ua_ui_session_properties_set_type(
        UAUiSessionProperties* properties,
        UAUiSessionType type);
    
#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_SESSION_H_ */
