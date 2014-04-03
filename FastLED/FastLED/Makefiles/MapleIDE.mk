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
# Last update: Apr 28, 2013 release 47

# MAPLE SUPPORT IS PUT ON HOLD
WARNING_MESSAGE = 'MAPLE SUPPORT IS PUT ON HOLD'


# Leaflabs Maple specifics
# ----------------------------------
#
# The Maple reset script —which sends control signals over
# the USB-serial connection to restart and enter the bootloader—
# is written in Python and requires the PySerial library. 
#
# Instructions available at http://leaflabs.com/docs/unix-toolchain.html#os-x
# Download PySerial library from http://pypi.python.org/pypi/pyserial
#
#
PLATFORM         := MapleIDE
PLATFORM_TAG      = MAPLE_IDE EMBEDXCODE=$(RELEASE_NOW)
APPLICATION_PATH := $(MAPLE_PATH)

UPLOADER          = dfu-util
DFU_UTIL_PATH     = $(APPLICATION_PATH)/hardware/tools/arm/bin
DFU_UTIL          = $(DFU_UTIL_PATH)/dfu-util
DFU_UTIL_OPTS     = -a$(call PARSE_BOARD,$(BOARD_TAG),upload.altID) -d $(call PARSE_BOARD,$(BOARD_TAG),upload.usbID)
DFU_RESET         = $(UTILITIES_PATH)/reset.py

APP_TOOLS_PATH   := $(APPLICATION_PATH)/hardware/tools/arm/bin
CORE_LIB_PATH    := $(APPLICATION_PATH)/hardware/leaflabs/cores/maple
APP_LIB_PATH     := $(APPLICATION_PATH)/libraries
BOARDS_TXT       := $(APPLICATION_PATH)/hardware/leaflabs/boards.txt

# Sketchbook/Libraries path
# wildcard required for ~ management
# ?ibraries required for libraries and Libraries
#
ifeq ($(USER_PATH)/Library/MapleIDE/preferences.txt,)
    $(error Error: run Mpide once and define the sketchbook path)
endif

ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    SKETCHBOOK_DIR = $(shell grep sketchbook.path $(USER_PATH)/Library/MapleIDE/preferences.txt | cut -d = -f 2)
endif

ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    $(error Error: sketchbook path not found)
endif

USER_LIB_PATH  = $(wildcard $(SKETCHBOOK_DIR)/?ibraries)
CORE_AS_SRCS   = $(wildcard $(CORE_LIB_PATH)/*.S) # */


# Rules for making a c++ file from the main sketch (.pde)
#
PDEHEADER      = \\\#include \"WProgram.h\"  


# Tool-chain names
#
CC      = $(APP_TOOLS_PATH)/arm-none-eabi-gcc
CXX     = $(APP_TOOLS_PATH)/arm-none-eabi-g++
AR      = $(APP_TOOLS_PATH)/arm-none-eabi-ar
OBJDUMP = $(APP_TOOLS_PATH)/arm-none-eabi-objdump
OBJCOPY = $(APP_TOOLS_PATH)/arm-none-eabi-objcopy
SIZE    = $(APP_TOOLS_PATH)/arm-none-eabi-size
NM      = $(APP_TOOLS_PATH)/arm-none-eabi-nm

lplm4f120h5qr.build.mcu=cortex-m4

BOARD    = $(call PARSE_BOARD,$(BOARD_TAG),build.board)
LDSCRIPT = $(call PARSE_BOARD,$(BOARD_TAG),build.linker)
VARIANT  = $(call PARSE_BOARD,$(BOARD_TAG),build.mcu)
#VARIANT_PATH = $(APPLICATION_PATH)/hardware/leaflabs/cores/maples/$(VARIANT)


MCU             = $(call PARSE_BOARD,$(BOARD_TAG),build.family)
MCU_FLAG_NAME   = mcpu

EXTRA_LDFLAGS   = -T$(CORE_LIB_PATH)/$(LDSCRIPT) -L$(APPLICATION_PATH)/hardware/leaflabs/cores/maple
EXTRA_LDFLAGS  += -mthumb -Xlinker --gc-sections --print-gc-sections --march=armv7-m

EXTRA_CPPFLAGS  = -fno-rtti -fno-exceptions -mthumb -march=armv7-m -nostdlib
EXTRA_CPPFLAGS += -DBOARD_$(BOARD) -DMCU_$(call PARSE_BOARD,$(BOARD_TAG),build.mcu)
EXTRA_CPPFLAGS += -D$(call PARSE_BOARD,$(BOARD_TAG),build.vect)
EXTRA_CPPFLAGS += -D$(call PARSE_BOARD,$(BOARD_TAG),build.density)
EXTRA_CPPFLAGS += -DERROR_LED_PORT=$(call PARSE_BOARD,$(BOARD_TAG),build.error_led_port)
EXTRA_CPPFLAGS += -DERROR_LED_PIN=$(call PARSE_BOARD,$(BOARD_TAG),build.error_led_pin)
EXTRA_CPPFLAGS += $(addprefix -D, $(PLATFORM_TAG))
    
OBJCOPYFLAGS  = -v -Obinary 
TARGET_HEXBIN = $(TARGET_BIN)

MAX_RAM_SIZE = $(call PARSE_BOARD,$(BOARD_TAG),upload.ram.maximum_size)





