#
# embedXcode
# ----------------------------------
# Embedded Computing on Xcode
#
# Copyright © Rei VILO, 2010-2014
# http://embedxcode.weebly.com
# All rights reserved
#
#
# Last update: Feb 01, 2014 release 129



# Teensy specifics
# ----------------------------------
#
PLATFORM         := Teensy
PLATFORM_TAG      = ARDUINO=105 TEENSYDUINO=117 EMBEDXCODE=$(RELEASE_NOW)
APPLICATION_PATH := $(TEENSY_PATH)


# Automatic Teensy2 or Teensy 3 selection based on build.core
#
BOARDS_TXT  := $(APPLICATION_PATH)/hardware/teensy/boards.txt
BUILD_CORE   = $(call PARSE_BOARD,$(BOARD_TAG),build.core)

ifeq ($(BUILD_CORE),teensy)
    include $(MAKEFILE_PATH)/Teensy2.mk
else ifeq ($(BUILD_CORE),teensy3)
    include $(MAKEFILE_PATH)/Teensy3.mk
else
    $(error $(BUILD_CORE) unknown) 
endif

# Teensy USB PID VID
#
USB_VID   := $(call PARSE_BOARD,$(BOARD_TAG),build.vid)
USB_PID   := $(call PARSE_BOARD,$(BOARD_TAG),build.pid)

ifneq ($(USB_PID),)
ifneq ($(USB_VID),)
    USB_FLAGS  = -DUSB_VID=$(USB_VID)
    USB_FLAGS += -DUSB_PID=$(USB_PID)
endif
endif

ifeq ($(USB_FLAGS),)
    USB_FLAGS = -DUSB_VID=null -DUSB_PID=null
endif

USB_FLAGS += -DUSB_SERIAL -DLAYOUT_US_ENGLISH -DTIME_T=$(shell date +%s)

MAX_RAM_SIZE = $(call PARSE_BOARD,$(BOARD_TAG),upload.maximum_ram_size)



