#ifndef UBUNTU_APPLICATION_UI_DISPLAY_H_
#define UBUNTU_APPLICATION_UI_DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UAUiDisplay;
    
    UAUiDisplay*
    ua_ui_display_new_with_index(
        size_t index);
    
    void
    ua_ui_display_destroy(
        UAUiDisplay* display);
    
    uint32_t
    ua_ui_display_query_horizontal_res(
        UAUiDisplay* display);
    
    uint32_t
    ua_ui_display_query_vertical_res(
        UAUiDisplay* display);
    
#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_DISPLAY_H_ */
