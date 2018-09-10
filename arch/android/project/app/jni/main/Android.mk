LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main
LOCAL_SRC_FILES := ../lib/$(TARGET_ARCH_ABI)/libmain.so
LOCAL_SHARED_LIBRARIES := SDL2 ogg vorbis

include $(PREBUILT_SHARED_LIBRARY)
