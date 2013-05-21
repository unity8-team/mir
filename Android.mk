LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=		\
    eglapp.c \
	startup_cost_sf.cpp 

LOCAL_MODULE:= startup_cost_sf
LOCAL_MODULE_TAGS := test

LOCAL_SHARED_LIBRARIES := libui libutils libgui libEGL libGLESv2

include $(BUILD_EXECUTABLE)
