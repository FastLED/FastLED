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
# Last update: Jan 20, 2014 release 125



# chipKIT MPIDE specifics
# ----------------------------------
# Dirty implementation for MPIDE release 0023-macosx-20130715
# OPT_SYSTEM_INTERNAL is defined in main.cpp but used in wiring.h
#
PLATFORM         := MPIDE
PLATFORM_TAG      = ARDUINO=23 MPIDE=23 MPIDEVER=0x01000202 EMBEDXCODE=$(RELEASE_NOW)

APPLICATION_PATH := $(MPIDE_PATH)

APP_TOOLS_PATH   := $(APPLICATION_PATH)/hardware/pic32/compiler/pic32-tools/bin
CORE_LIB_PATH    := $(APPLICATION_PATH)/hardware/pic32/cores/pic32
APP_LIB_PATH     := $(APPLICATION_PATH)/hardware/pic32/libraries
BOARDS_TXT       := $(APPLICATION_PATH)/hardware/pic32/boards.txt

# Sketchbook/Libraries path
# wildcard required for ~ management
# ?ibraries required for libraries and Libraries
#
ifeq ($(wildcard $(USER_PATH)/Library/Mpide/preferences.txt),)
    $(error Error: run Mpide once and define the sketchbook path)
endif

ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    SKETCHBOOK_DIR = $(shell grep sketchbook.path $(USER_PATH)/Library/Mpide/preferences.txt | cut -d = -f 2)
endif
ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    $(error Error: sketchbook path not found)
endif
USER_LIB_PATH  = $(wildcard $(SKETCHBOOK_DIR)/?ibraries)


# Rules for making a c++ file from the main sketch (.pde)
#
PDEHEADER      = \\\#include \"WProgram.h\"  


# Add .S files required by MPIDE release 0023-macosx-20130715
#
CORE_AS_SRCS   = $(wildcard $(CORE_LIB_PATH)/*.S) # */


# Tool-chain names
#
CC      = $(APP_TOOLS_PATH)/pic32-gcc
CXX     = $(APP_TOOLS_PATH)/pic32-g++
AR      = $(APP_TOOLS_PATH)/pic32-ar
OBJDUMP = $(APP_TOOLS_PATH)/pic32-objdump
OBJCOPY = $(APP_TOOLS_PATH)/pic32-objcopy
SIZE    = $(APP_TOOLS_PATH)/pic32-size
NM      = $(APP_TOOLS_PATH)/pic32-nm


BOARD    = $(call PARSE_BOARD,$(BOARD_TAG),board)
LDSCRIPT = $(call PARSE_BOARD,$(BOARD_TAG),ldscript)
VARIANT  = $(call PARSE_BOARD,$(BOARD_TAG),build.variant)
VARIANT_PATH = $(APPLICATION_PATH)/hardware/pic32/variants/$(VARIANT)

MCU_FLAG_NAME  = mprocessor
# chipKIT-application-COMMON.ld added by MPIDE release 0023-macosx-20130715
EXTRA_LDFLAGS  = -T$(CORE_LIB_PATH)/$(LDSCRIPT) -T$(CORE_LIB_PATH)/chipKIT-application-COMMON.ld
EXTRA_CPPFLAGS = -mdebugger -Wcast-align -O2 -mno-smart-io -G1024 $(addprefix -D, $(PLATFORM_TAG)) -D$(BOARD) -I$(VARIANT_PATH)

