LOCAL_PATH := $(call my-dir)

# NDK_DEBUG_IMPORTS := 1

#########################################################
# Crypto++ library
include $(CLEAR_VARS)

CRYPTOPP_INCL   := /usr/local/cryptopp/android-$(TARGET_ARCH_ABI)/include
CRYPTOPP_LIB    := /usr/local/cryptopp/android-$(TARGET_ARCH_ABI)/lib

LOCAL_MODULE       := cryptopp
LOCAL_SRC_FILES    := $(CRYPTOPP_LIB)/libcryptopp_static.a
LOCAL_CPP_FEATURES := rtti exceptions

LOCAL_EXPORT_C_INCLUDES := $(CRYPTOPP_INCL) $(CRYPTOPP_INCL)/cryptopp

include $(PREBUILT_SHARED_LIBRARY)

LOCAL_SHARED_LIBRARIES  := cryptopp

#########################################################
# PRNG library
include $(CLEAR_VARS)

LOCAL_MODULE := prng
LOCAL_SRC_FILES := $(addprefix $(CRYPTOPP_PATH),test_shared.cxx)
LOCAL_CPPFLAGS := -Wall -fvisibility=hidden
LOCAL_CPP_FEATURES := rtti exceptions
LOCAL_LDFLAGS := -Wl,--exclude-libs,ALL -Wl,--as-needed

# Configure for release unless NDK_DEBUG=1
ifeq ($(NDK_DEBUG),1)
    LOCAL_CPPFLAGS := $(LOCAL_CPPFLAGS) -DDEBUG
else
    LOCAL_CPPFLAGS := $(LOCAL_CPPFLAGS) -DNDEBUG
endif

LOCAL_EXPORT_CPPFLAGS := $(LOCAL_CPPFLAGS)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/..
LOCAL_EXPORT_LDFLAGS := -Wl,--gc-sections

LOCAL_STATIC_LIBRARIES := cryptopp_static

include $(BUILD_SHARED_LIBRARY)
