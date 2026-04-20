LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libbmlib

LOCAL_SRC_FILES := \
    src/bmkernel_runtime.cpp \
    src/bmlib_mmpool.cpp \
    src/bmcpu_runtime.cpp \
    src/bmlib_runtime.cpp \
    src/bmlib_util.cpp \
    src/bmlib_log.cpp \
    src/bmlib_device.cpp \
    src/bmlib_memory.cpp \
    src/a53lite_api.cpp \
    src/bmlib_profile.cpp \
    src/linux/bmlib_ioctl.cpp \
    src/bmlib_md5.cpp \
    src/rbtree.c \
    src/fw_log.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/src \
    $(LOCAL_PATH)/src/linux \
    $(LOCAL_PATH)/include

LOCAL_CFLAGS := \
    -DUSING_INT_CDMA=1 \
    -Wall \
    -Werror \
    -Wno-unused-parameter \
    -Wno-unused-variable \
    -Wno-self-assign \
    -Wno-constant-conversion \
    -Wno-writable-strings \
    -DUSE_IN_LINUX

LOCAL_CPPFLAGS := \
    -fexceptions \
    -std=gnu++11 \
    -DUSE_IN_LINUX

LOCAL_SHARED_LIBRARIES := \
    libdl \
    liblog \
    libcutils

LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS_arm64:=  -D__linux__
LOCAL_CFLAGS_arm32:=  -D__linux__

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := test_setup_veth

LOCAL_SRC_FILES := tools/bmcpu/src/test_setup_veth.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/src

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -DUSE_IN_LINUX


LOCAL_SHARED_LIBRARIES := libbmlib

LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)
