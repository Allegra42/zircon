LOCAL_DIR = $(GET_LOCAL_DIR)


MODULE := $(LOCAL_DIR).rgb

MODULE_NAME := grove-cpp-rgb-drv

MODULE_TYPE := driver

MODULE_SRCS := \
	$(LOCAL_DIR)/bind-rgb.c \
	$(LOCAL_DIR)/grove-rgb.cpp \

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
	system/banjo/ddk-protocol-i2c \
	system/banjo/ddk-protocol-platform-bus \
	system/banjo/ddk-protocol-platform-device \

MODULE_FIDL_LIBS := \
	system/fidl/zircon-display-grove.rgb \

include make/module.mk



# MODULE := $(LOCAL_DIR).lcd
# 
# MODULE_NAME := grove-cpp-drv.lcd
# 
# MODULE_TYPE := driver
# 
# MODULE_SRCS := \
	# $(LOCAL_DIR)/bind-lcd.c \
	# $(LOCAL_DIR)/grove-lcd.cpp \
# 
# MODULE_STATIC_LIBS := \
	# system/ulib/ddk \
	# system/ulib/ddktl \
	# system/ulib/fbl \
	# system/ulib/fidl \
	# system/ulib/sync \
	# system/ulib/zx \
	# system/ulib/zxcpp \
# 
# MODULE_LIBS := \
	# system/ulib/driver \
	# system/ulib/zircon \
	# system/ulib/c \
# 
# MODULE_BANJO_LIBS := \
	# system/banjo/ddk-protocol-i2c \
	# system/banjo/ddk-protocol-platform-bus \
	# system/banjo/ddk-protocol-platform-device \
# 
# MODULE_FIDL_LIBS := \
	# system/fidl/zircon-display-grove.lcd \
# 
# include make/module.mk
