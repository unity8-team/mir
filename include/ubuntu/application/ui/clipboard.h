#ifndef UBUNTU_APPLICATION_UI_CLIPBOARD_H_
#define UBUNTU_APPLICATION_UI_CLIPBOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

    void
    ua_ui_set_clipboard_content(
        void* data,
        size_t size);
    
    void
    ua_ui_get_clipboard_content(
        void** data,
        size_t* size);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_CLIPBOARD_H_ */
