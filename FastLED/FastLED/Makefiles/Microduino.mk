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
# Last update: Jan 04, 2014 release 122



# Microduino 1.0.x AVR specifics
# ----------------------------------
#
PLATFORM         := Microduino
BUILD_CORE       := avr
PLATFORM_TAG      = ARDUINO=105 MICRODUINO EMBEDXCODE=$(RELEASE_NOW)
APPLICATION_PATH := $(MICRODUINO_PATH)

APP_TOOLS_PATH   := $(APPLICATION_PATH)/hardware/tools/avr/bin
CORE_LIB_PATH    := $(APPLICATION_PATH)/hardware/Microduino/cores/arduino
APP_LIB_PATH     := $(APPLICATION_PATH)/libraries
BOARDS_TXT       := $(APPLICATION_PATH)/hardware/Microduino/boards.txt


# Sketchbook/Libraries path
# wildcard required for ~ management
#
ifeq ($(USER_PATH)/Library/Arduino/preferences.txt,)
    $(error Error: run Microduino or Arduino once and define the sketchbook path)
endif

ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    SKETCHBOOK_DIR = $(shell grep sketchbook.path $(USER_PATH)/Library/Arduino/preferences.txt | cut -d = -f 2)
endif
ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    $(error Error: sketchbook path not found)
endif
USER_LIB_PATH  = $(wildcard $(SKETCHBOOK_DIR)/?ibraries)

# Rules for making a c++ file from the main sketch (.pde)
#
PDEHEADER      = \\\#include \"Arduino.h\"

# Tool-chain names
#
CC      = $(APP_TOOLS_PATH)/avr-gcc
CXX     = $(APP_TOOLS_PATH)/avr-g++
AR      = $(APP_TOOLS_PATH)/avr-ar
OBJDUMP = $(APP_TOOLS_PATH)/avr-objdump
OBJCOPY = $(APP_TOOLS_PATH)/avr-objcopy
SIZE    = $(APP_TOOLS_PATH)/avr-size
NM      = $(APP_TOOLS_PATH)/avr-nm

# Specific AVRDUDE location and options
#
AVRDUDE_COM_OPTS  = -D -p$(MCU) -C$(AVRDUDE_CONF)

BOARD        = $(call PARSE_BOARD,$(BOARD_TAG),board)
#LDSCRIPT     = $(call PARSE_BOARD,$(BOARD_TAG),ldscript)
VARIANT      = $(call PARSE_BOARD,$(BOARD_TAG),build.variant)
VARIANT_PATH = $(APPLICATION_PATH)/hardware/Microduino/variants/$(VARIANT)

MCU_FLAG_NAME  = mmcu
EXTRA_LDFLAGS  =
EXTRA_CPPFLAGS = -MMD -I$(VARIANT_PATH) $(addprefix -D, $(PLATFORM_TAG))

# Leonardo USB PID VID
#
USB_TOUCH := $(call PARSE_BOARD,$(BOARD_TAG),upload.protocol)
USB_VID   := $(call PARSE_BOARD,$(BOARD_TAG),build.vid)
USB_PID   := $(call PARSE_BOARD,$(BOARD_TAG),build.pid)

ifneq ($(USB_PID),)
    USB_FLAGS += -DUSB_PID=$(USB_PID)
else
    USB_FLAGS += -DUSB_PID=null
endif

ifneq ($(USB_VID),)
    USB_FLAGS += -DUSB_VID=$(USB_VID)
else
    USB_FLAGS += -DUSB_VID=null
endif

# Serial 1200 reset
#
ifeq ($(USB_TOUCH),avr109)
    USB_RESET  = $(UTILITIES_PATH)/serial1200.py
endif
