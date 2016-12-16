LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=   \
    main.c   \
    getpss.c   \
    hash.c     \
    getmem.c   \
    error.c

LOCAL_MODULE:= meminfo

LOCAL_MODULE_TAGS := tests

LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32 := meminfo
LOCAL_MODULE_STEM_64 := meminfo
LOCAL_CFLAGS += -DANDROID
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \

include $(BUILD_EXECUTABLE)
