LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := fidl

MODULE_PACKAGE := fidl

MODULE_FIDL_LIBRARY := zircon.display.grove.rgb

MODULE_SRCS += $(LOCAL_DIR)/grove-rgb.fidl

include make/module.mk

