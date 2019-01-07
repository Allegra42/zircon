LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR).rgb

MODULE_NAME := grove-lcd-rgb-drv

MODULE_TYPE = driver

MODULE_SRCS := $(LOCAL_DIR)/grove-rgb.c

MODULE_STATIC_LIBS := system/ulib/ddk system/ulib/fidl system/ulib/sync

MODULE_LIBS := system/ulib/driver system/ulib/c system/ulib/zircon

MODULE_BANJO_LIBS := system/banjo/ddk-protocol-i2c system/banjo/ddk-protocol-platform-device system/banjo/ddk-protocol-platform-bus

MODULE_FIDL_LIBS := system/fidl/zircon-display-grove-rgb

include make/module.mk
