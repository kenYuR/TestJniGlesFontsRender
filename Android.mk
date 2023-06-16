LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libfreetype
LOCAL_SRC_FILES := libfreetype.a 
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := JniGlesFontsRender
LOCAL_SRC_FILES := JniGlesFontsRender.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/freetype2
LOCAL_LDLIBS := -llog -ldl -lmediandk -landroid -lEGL -lGLESv2 -lGLESv3 -lz
LOCAL_CXXFLAGS := -std=c++11
LOCAL_STATIC_LIBRARIES := libfreetype
include $(BUILD_SHARED_LIBRARY)