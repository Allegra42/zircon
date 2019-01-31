LOCAL_DIR := $(GET_LOCAL_DIR)


MODULE := $(LOCAL_DIR).rgb

MODULE_TYPE := fidl

MODULE_PACKAGE := fidl

MODULE_FIDL_LIBRARY := zircon.display.grove.rgb

MODULE_SRCS += $(LOCAL_DIR)/grove-rgb.fidl

include make/module.mk


MODULE := $(LOCAL_DIR).lcd

MODULE_TYPE := fidl

MODULE_PACKAGE := fidl

MODULE_FIDL_LIBRARY := zircon.display.grove.lcd

MODULE_SRCS += $(LOCAL_DIR)/grove-lcd.fidl

include make/module.mk


MODULE := $(LOCAL_DIR).pdev

MODULE_TYPE := fidl

MODULE_PACKAGE := fidl

MODULE_FIDL_LIBRARY := zircon.display.grove.pdev

MODULE_SRCS += $(LOCAL_DIR)/grove.fidl

include make/module.mk
