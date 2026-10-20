LOCAL_PATH:= $(call my-dir)

    # s2d daemon: an executable, built by the buildS2d task in app/build.gradle.kts and packaged as libs2d.so
    # (ndk-build won't give an executable a .so name, and AGP only packages lib*.so).

    # 6 Tuner Plugins:

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= tnr/tnr_ssl.c
LOCAL_MODULE:= libs2t_ssl
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= tnr/tnr_qcv.c
LOCAL_MODULE:= libs2t_qcv
LOCAL_LDLIBS := -llog
#LOCAL_SHARED_LIBRARIES := libnativehelper libcutils
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= tnr/tnr_bch.c
LOCAL_MODULE:= libs2t_bch
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= tnr/tnr_mtk.c
LOCAL_MODULE:= libs2t_mtk
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= tnr/tnr_cus.c
LOCAL_MODULE:= libs2t_cus
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

    # Bluetooth libbt-vendor shim:
include $(CLEAR_VARS)
LOCAL_MODULE    := libbt-vendor
LOCAL_SRC_FILES := bts/bt-ven.c
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

    # Bluetooth libbt-hci shim:
include $(CLEAR_VARS)
LOCAL_MODULE    := libbt-hci
LOCAL_SRC_FILES := bts/bt-hci.c
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

    # Disable TEST:
#include $(CLEAR_VARS)
#LOCAL_MODULE:= libbt-hci
#LOCAL_SRC_FILES := libbt-hci.so
#include $(PREBUILT_SHARED_LIBRARY)

    # JNI utilities
include $(CLEAR_VARS)
LOCAL_MODULE:= jut
LOCAL_SRC_FILES:= jut/jut.c
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

