LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR) 

MODULE_TYPE := userapp

MODULE_GROUP := misc

MODULE_SRCS += $(LOCAL_DIR)/grove-uapp.c

MODULE_LIBS := system/ulib/fdio system/ulib/c system/ulib/zircon

MODULE_FIDL_LIBS := system/fidl/zircon-display-grove.rgb system/fidl/zircon-display-grove.lcd

include make/module.mk




MODULE := $(LOCAL_DIR).pdev

MODULE_TYPE := userapp

MODULE_GROUP := misc

MODULE_SRCS += $(LOCAL_DIR)/grove-uapp-pdev.c

MODULE_LIBS := system/ulib/fdio system/ulib/c system/ulib/zircon

MODULE_FIDL_LIBS := system/fidl/zircon-display-grove.pdev

include make/module.mk
