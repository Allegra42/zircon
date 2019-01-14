LOCAL_DIR := $(GET_LOCAL_DIR)


# Makefile for the RGB driver
MODULE := $(LOCAL_DIR).rgb

MODULE_NAME := grove-rgb-drv

MODULE_TYPE = driver

MODULE_SRCS := $(LOCAL_DIR)/grove-rgb.c

MODULE_STATIC_LIBS := system/ulib/ddk system/ulib/fidl system/ulib/sync

MODULE_LIBS := system/ulib/driver system/ulib/c system/ulib/zircon

MODULE_BANJO_LIBS := system/banjo/ddk-protocol-i2c system/banjo/ddk-protocol-platform-device system/banjo/ddk-protocol-platform-bus

MODULE_FIDL_LIBS := system/fidl/zircon-display-grove-rgb

include make/module.mk


# Makefile for the LCD driver
MODULE := $(LOCAL_DIR).lcd

MODULE_NAME := grove-lcd-drv

MODULE_TYPE = driver

MODULE_SRCS := $(LOCAL_DIR)/grove-lcd.c

MODULE_STATIC_LIBS := system/ulib/ddk system/ulib/fidl system/ulib/sync

MODULE_LIBS := system/ulib/driver system/ulib/c system/ulib/zircon

MODULE_BANJO_LIBS := system/banjo/ddk-protocol-i2c system/banjo/ddk-protocol-platform-device system/banjo/ddk-protocol-platform-bus

# MODULE_FIDL_LIBS := system/fidl/zircon-display-grove-lcd

include make/module.mk
