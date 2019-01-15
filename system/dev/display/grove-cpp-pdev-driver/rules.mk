LOCAL_DIR = $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := driver

MODULE_SRCS += \
	$(LOCAL_DIR)/bind.c \
	$(LOCAL_DIR)/grove.cpp \

MODULE_STATIC_LIBS := \
    system/ulib/ddk \
    system/ulib/ddktl \
    system/ulib/fbl \
    system/ulib/fidl \
    system/ulib/sync \
    system/ulib/zx \
    system/ulib/zxcpp \

MODULE_LIBS := \
    system/ulib/driver \
    system/ulib/zircon \
    system/ulib/c \

MODULE_BANJO_LIBS := \
	system/banjo/ddk-protocol-gpio \
	system/banjo/ddk-protocol-i2c \
	system/banjo/ddk-protocol-platform-bus \
    system/banjo/ddk-protocol-platform-device \

MODULE_FIDL_LIBS := system/fidl/zircon-display-grove-rgb

include make/module.mk
