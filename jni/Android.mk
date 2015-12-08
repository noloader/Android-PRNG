LOCAL_PATH := $(call my-dir)

# NDK_DEBUG_IMPORTS := 1

#########################################################
# STLport library
include $(CLEAR_VARS)

STLPORT_INCL     := /opt/android-ndk-r10d/sources/cxx-stl/stlport/stlport
STLPORT_LIB      := /opt/android-ndk-r10d/sources/cxx-stl/stlport/libs/$(TARGET_ARCH_ABI)

LOCAL_MODULE := stlport_shared
LOCAL_SRC_FILES := $(STLPORT_LIB)/libstlport_shared.so

LOCAL_EXPORT_CPPFLAGS :=
LOCAL_EXPORT_C_INCLUDES := $(STLPORT_INCL)

include $(PREBUILT_SHARED_LIBRARY)

LOCAL_SHARED_LIBRARIES  := stlport_shared

#########################################################
# Crypto++ library
include $(CLEAR_VARS)

CRYPTOPP_INCL   := /usr/local/cryptopp/android-$(TARGET_ARCH_ABI)/include
CRYPTOPP_LIB    := /usr/local/cryptopp/android-$(TARGET_ARCH_ABI)/lib

LOCAL_MODULE       := cryptopp
LOCAL_SRC_FILES    := $(CRYPTOPP_LIB)/libcryptopp.so

LOCAL_EXPORT_CPPFLAGS := -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
LOCAL_EXPORT_C_INCLUDES := $(CRYPTOPP_INCL) $(CRYPTOPP_INCL)/cryptopp

include $(PREBUILT_SHARED_LIBRARY)

LOCAL_SHARED_LIBRARIES  := cryptopp

#########################################################
# PRNG library
include $(CLEAR_VARS)

APP_STL         := stlport_shared
APP_MODULES     := prng stlport_shared cryptopp

# My ass... LOCAL_EXPORT_C_INCLUDES is useless
LOCAL_C_INCLUDES   := $(STLPORT_INCL) $(CRYPTOPP_INCL)

LOCAL_CPP_FEATURES := rtti exceptions

LOCAL_CPP_FLAGS    := -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
LOCAL_CPP_FLAGS    += -Wl,--exclude-libs,ALL

LOCAL_LDLIBS            := -llog -landroid
LOCAL_SHARED_LIBRARIES  := cryptopp stlport_shared

LOCAL_MODULE    := prng
LOCAL_SRC_FILES := libprng.cpp

include $(BUILD_SHARED_LIBRARY)
