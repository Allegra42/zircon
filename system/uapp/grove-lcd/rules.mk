LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := userapp
MODULE_GROUP := misc

MODULE_SRCS += $(LOCAL_DIR)/main.c

MODULE_LIBS := system/ulib/fdio system/ulib/c system/ulib/zircon

MODULE_FIDL_LIBS := system/fidl/zircon-display-grove-rgb

include make/module.mk
