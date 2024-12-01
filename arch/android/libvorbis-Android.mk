LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libvorbis
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/lib
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libogg

# https://developer.android.com/guide/practices/page-sizes
LOCAL_LDFLAGS += "-Wl,-z,max-page-size=16384"

LOCAL_SRC_FILES := lib/mdct.c lib/smallft.c lib/block.c lib/envelope.c lib/window.c lib/lsp.c \
			lib/lpc.c lib/analysis.c lib/synthesis.c lib/psy.c lib/info.c \
			lib/floor1.c lib/floor0.c \
			lib/res0.c lib/mapping0.c lib/registry.c lib/codebook.c lib/sharedbook.c \
			lib/lookup.c lib/bitrate.c

include $(BUILD_SHARED_LIBRARY)
include $(CLEAR_VARS)

LOCAL_MODULE := libvorbisfile
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/lib
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libogg libvorbis

# https://developer.android.com/guide/practices/page-sizes
LOCAL_LDFLAGS += "-Wl,-z,max-page-size=16384"

LOCAL_SRC_FILES := lib/vorbisfile.c

include $(BUILD_SHARED_LIBRARY)
