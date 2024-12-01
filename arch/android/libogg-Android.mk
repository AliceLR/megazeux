LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libogg
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

# https://developer.android.com/guide/practices/page-sizes
LOCAL_LDFLAGS += "-Wl,-z,max-page-size=16384"

LOCAL_SRC_FILES := src/bitwise.c src/framing.c

include $(BUILD_SHARED_LIBRARY)
