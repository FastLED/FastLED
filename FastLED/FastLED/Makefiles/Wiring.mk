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
# Last update: Mar 01, 2014 release 136

# WIRING SUPPORT IS PUT ON HOLD
WARNING_MESSAGE = 'WIRING SUPPORT IS PUT ON HOLD'


# Wiring specifics
# ----------------------------------
#
PLATFORM         := Wiring
PLATFORM_TAG      = WIRING=100 EMBEDXCODE=$(RELEASE_NOW)
APPLICATION_PATH := $(WIRING_PATH)

APP_TOOLS_PATH   := $(APPLICATION_PATH)/tools/avr/bin
CORE_LIB_PATH    := $(APPLICATION_PATH)/cores/Common
APP_LIB_PATH     := $(APPLICATION_PATH)/libraries
BOARDS_TXT       := $(APPLICATION_PATH)/hardware/Wiring/boards.txt

# Sketchbook/Libraries path
# wildcard required for ~ management
# ?ibraries required for libraries and Libraries
#
ifeq ($(USER_PATH)/Library/Wiring/preferences.txt,)
    $(error Error: run Wiring once and define the sketchbook path)
endif

ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    SKETCHBOOK_DIR = $(shell grep sketchbook.path $(USER_PATH)/Library/Wiring/preferences.txt | cut -d = -f 2)
endif
ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    $(error Error: sketchbook path not found)
endif
USER_LIB_PATH  = $(wildcard $(SKETCHBOOK_DIR)/?ibraries)

# Rules for making a c++ file from the main sketch (.pde)
#
PDEHEADER      = \\\#include \"Wiring.h\"  

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
AVRDUDE_PATH      = $(APPLICATION_PATH)/tools/avr
AVRDUDE           = $(AVRDUDE_PATH)/bin/avrdude
AVRDUDE_CONF      = $(AVRDUDE_PATH)/bin/avrdude.conf
AVRDUDE_COM_OPTS  = -D -p$(MCU) -C$(AVRDUDE_CONF)

BOARD        = $(call PARSE_BOARD,$(BOARD_TAG),board)
#LDSCRIPT = $(call PARSE_BOARD,$(BOARD_TAG),ldscript)
VARIANT      = $(call PARSE_BOARD,$(BOARD_TAG),build.hardware)
VARIANT_PATH = $(APPLICATION_PATH)/hardware/Wiring/$(VARIANT)
BUILD_CORE   = $(call PARSE_BOARD,$(BOARD_TAG),build.core)

BUILD_CORE_LIB_PATH  = $(APPLICATION_PATH)/cores/$(BUILD_CORE)
BUILD_CORE_LIBS_LIST = $(subst .h,,$(subst $(BUILD_CORE_LIB_PATH)/,,$(wildcard $(BUILD_CORE_LIB_PATH)/*.h))) # */

BUILD_CORE_C_SRCS    = $(wildcard $(BUILD_CORE_LIB_PATH)/*.c) # */

ifneq ($(strip $(NO_CORE_MAIN_FUNCTION)),)
    BUILD_CORE_CPP_SRCS = $(filter-out %program.cpp %main.cpp,$(wildcard $(BUILD_CORE_LIB_PATH)/*.cpp)) # */
else
    BUILD_CORE_CPP_SRCS = $(filter-out %program.cpp, $(wildcard $(BUILD_CORE_LIB_PATH)/*.cpp)) # */
endif

BUILD_CORE_OBJ_FILES  = $(BUILD_CORE_C_SRCS:.c=.o) $(BUILD_CORE_CPP_SRCS:.cpp=.o)
BUILD_CORE_OBJS       = $(patsubst $(BUILD_CORE_LIB_PATH)/%,$(OBJDIR)/%,$(BUILD_CORE_OBJ_FILES))

# Extra variant library
#
VARIANT_CPP_SRC = $(wildcard $(VARIANT_PATH)/*.cpp)
VARIANT_OBJS    = $(patsubst $(VARIANT_PATH)/%.cpp,$(OBJDIR)/libs/%.o,$(VARIANT_CPP_SRC)) # */

# Two locations for Wiring libraries
#
BUILD_APP_LIB_PATH = $(BUILD_CORE_LIB_PATH)/libraries

ifndef APP_LIBS_LIST
    w1             = $(realpath $(sort $(dir $(wildcard $(APP_LIB_PATH)/*/*.h $(APP_LIB_PATH)/*/*/*.h)))) # */
    APP_LIBS_LIST  = $(subst $(APP_LIB_PATH)/,,$(filter-out $(EXCLUDE_LIST),$(w1)))

    w2             = $(realpath $(sort $(dir $(wildcard $(BUILD_APP_LIB_PATH)/*/*.h $(BUILD_APP_LIB_PATH)/*/*/*.h)))) # */
    BUILD_APP_LIBS_LIST = $(subst $(BUILD_APP_LIB_PATH)/,,$(filter-out $(EXCLUDE_LIST),$(w2)))
else
    BUILD_APP_LIBS_LIST = $(APP_LIBS_LIST)
endif

ifneq ($(APP_LIBS_LIST),0)
    APP_LIBS        = $(patsubst %,$(APP_LIB_PATH)/%,$(APP_LIBS_LIST))
    APP_LIB_CPP_SRC = $(wildcard $(patsubst %,%/*.cpp,$(APP_LIBS))) # */
    APP_LIB_C_SRC   = $(wildcard $(patsubst %,%/*.c,$(APP_LIBS))) # */

    APP_LIB_OBJS    = $(patsubst $(APP_LIB_PATH)/%.cpp,$(OBJDIR)/libs/%.o,$(APP_LIB_CPP_SRC))
    APP_LIB_OBJS   += $(patsubst $(APP_LIB_PATH)/%.c,$(OBJDIR)/libs/%.o,$(APP_LIB_C_SRC))

    BUILD_APP_LIBS        = $(patsubst %,$(BUILD_APP_LIB_PATH)/%,$(BUILD_APP_LIBS_LIST))
    BUILD_APP_LIB_CPP_SRC = $(wildcard $(patsubst %,%/*.cpp,$(BUILD_APP_LIBS))) # */
    BUILD_APP_LIB_C_SRC   = $(wildcard $(patsubst %,%/*.c,$(BUILD_APP_LIBS))) # */

    BUILD_APP_LIB_OBJS    = $(patsubst $(BUILD_APP_LIB_PATH)/%.cpp,$(OBJDIR)/libs/%.o,$(BUILD_APP_LIB_CPP_SRC))
    BUILD_APP_LIB_OBJS   += $(patsubst $(BUILD_APP_LIB_PATH)/%.c,$(OBJDIR)/libs/%.o,$(BUILD_APP_LIB_C_SRC))
endif

MCU_FLAG_NAME  = mmcu
EXTRA_LDFLAGS  = 
EXTRA_CPPFLAGS = $(addprefix -D, $(PLATFORM_TAG)) -I$(CORE_LIB_PATH) -I$(BUILD_CORE_LIB_PATH) -I$(VARIANT_PATH) 
