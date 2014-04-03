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
# Last update: Feb 10, 2014 release 132

# ARDUINO 1.5.X IS STILL IN BETA, UNSTABLE AND PRONE TO BUGS
WARNING_MESSAGE = 'ARDUINO 1.5.X IS STILL IN BETA, UNSTABLE AND PRONE TO BUGS'


# Arduino 1.5.x SAM specifics
# ----------------------------------
#
PLATFORM         := Arduino
BUILD_CORE       := sam
PLATFORM_TAG      = ARDUINO=155 ARDUINO_ARCH_SAM EMBEDXCODE=$(RELEASE_NOW)
APPLICATION_PATH := $(ARDUINO_PATH)

APP_TOOLS_PATH   := $(APPLICATION_PATH)/hardware/tools/g++_arm_none_eabi/bin
CORE_LIB_PATH    := $(APPLICATION_PATH)/hardware/arduino/sam/cores/arduino
APP_LIB_PATH     := $(APPLICATION_PATH)/libraries
BOARDS_TXT       := $(APPLICATION_PATH)/hardware/arduino/sam/boards.txt

# 
# Uploader bossac 
# Tested by Mike Roberts 
#
UPLOADER          = bossac
BOSSAC_PATH       = $(APPLICATION_PATH)/hardware/tools
BOSSAC            = $(BOSSAC_PATH)/bossac
BOSSAC_PORT       = $(subst /dev/,,$(AVRDUDE_PORT))
BOSSAC_OPTS       = --port=$(BOSSAC_PORT) -U false -e -w -v -b

# Sketchbook/Libraries path
# wildcard required for ~ management
# ?ibraries required for libraries and Libraries
#
ifeq ($(USER_PATH)/Library/Arduino/preferences.txt,)
    $(error Error: run Arduino once and define the sketchbook path)
endif

ifeq ($(wildcard $(SKETCHBOOK_DIR)),)
    SKETCHBOOK_DIR = $(shell grep sketchbook.path $(USER_PATH)/Library/Arduino15/preferences.txt | cut -d = -f 2)
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
CC      = $(APP_TOOLS_PATH)/arm-none-eabi-gcc
CXX     = $(APP_TOOLS_PATH)/arm-none-eabi-g++
AR      = $(APP_TOOLS_PATH)/arm-none-eabi-ar
OBJDUMP = $(APP_TOOLS_PATH)/arm-none-eabi-objdump
OBJCOPY = $(APP_TOOLS_PATH)/arm-none-eabi-objcopy
SIZE    = $(APP_TOOLS_PATH)/arm-none-eabi-size
NM      = $(APP_TOOLS_PATH)/arm-none-eabi-nm

# Specific AVRDUDE location and options
#
AVRDUDE_COM_OPTS  = -D -p$(MCU) -C$(AVRDUDE_CONF)

BOARD    = $(call PARSE_BOARD,$(BOARD_TAG),board)
LDSCRIPT = $(call PARSE_BOARD,$(BOARD_TAG),build.ldscript)
VARIANT  = $(call PARSE_BOARD,$(BOARD_TAG),build.variant)
VARIANT_PATH = $(APPLICATION_PATH)/hardware/arduino/sam/variants/$(VARIANT)
VARIANT_CPP_SRCS  = $(wildcard $(VARIANT_PATH)/*.cpp) # */  $(VARIANT_PATH)/*/*.cpp #*/
VARIANT_OBJ_FILES = $(VARIANT_CPP_SRCS:.cpp=.o)
VARIANT_OBJS      = $(patsubst $(VARIANT_PATH)/%,$(OBJDIR)/%,$(VARIANT_OBJ_FILES))

SYSTEM_LIB  = $(call PARSE_BOARD,$(BOARD_TAG),build.variant_system_lib)
SYSTEM_PATH = $(VARIANT_PATH)
SYSTEM_OBJS = $(SYSTEM_PATH)/$(SYSTEM_LIB)


# Two locations for Arduino libraries
#
BUILD_APP_LIB_PATH  = $(APPLICATION_PATH)/hardware/arduino/$(BUILD_CORE)/libraries

ifndef APP_LIBS_LIST
    w1             = $(realpath $(sort $(dir $(wildcard $(APP_LIB_PATH)/*/*.h $(APP_LIB_PATH)/*/*/*.h)))) # */
    APP_LIBS_LIST  = $(subst $(APP_LIB_PATH)/,,$(filter-out $(EXCLUDE_LIST),$(w1)))

    w2             = $(realpath $(sort $(dir $(wildcard $(BUILD_APP_LIB_PATH)/*/*.h $(BUILD_APP_LIB_PATH)/*/*/*.h)))) # */
    BUILD_APP_LIBS_LIST = $(subst $(BUILD_APP_LIB_PATH)/,,$(filter-out $(EXCLUDE_LIST),$(w2)))
else
    w2             = $(realpath $(sort $(dir $(wildcard $(BUILD_APP_LIB_PATH)/*/*.h $(BUILD_APP_LIB_PATH)/*/*/*.h)))) # */
    BUILD_APP_LIBS_LIST = $(subst $(BUILD_APP_LIB_PATH)/,,$(filter-out $(EXCLUDE_LIST),$(w2)))
endif


# Arduino 1.5.x nightmare with src and arch/sam or arch/avr
# Another example of Arduino's quick and dirty job
#
ifneq ($(APP_LIBS_LIST),0)
    APP_LIBS        = $(patsubst %,$(APP_LIB_PATH)/%/src,$(APP_LIBS_LIST))
    APP_LIBS       += $(patsubst %,$(APP_LIB_PATH)/%/arch/$(BUILD_CORE),$(APP_LIBS_LIST))

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

SYSTEM_FLAGS    = -I$(APPLICATION_PATH)/hardware/arduino/sam/system/libsam 
SYSTEM_FLAGS   += -I$(APPLICATION_PATH)/hardware/arduino/sam/system/libsam/include
SYSTEM_FLAGS   += -I$(APPLICATION_PATH)/hardware/arduino/sam/system/CMSIS/CMSIS/Include/
SYSTEM_FLAGS   += -I$(APPLICATION_PATH)/hardware/arduino/sam/system/CMSIS/Device/ATMEL/

MCU_FLAG_NAME   = mcpu
EXTRA_LDFLAGS   = -T$(VARIANT_PATH)/$(LDSCRIPT) -Wl,-Map,Builds/embeddedcomputing.map $(VARIANT_OBJS)
EXTRA_LDFLAGS  += -lgcc -mthumb -Wl,--cref -Wl,--check-sections -Wl,--gc-sections -Wl,--entry=Reset_Handler 
EXTRA_LDFLAGS  += -Wl,--unresolved-symbols=report-all -Wl,--warn-common -Wl,--warn-section-align 
EXTRA_LDFLAGS  += -Wl,--warn-unresolved-symbols

EXTRA_CPPFLAGS  = -I$(VARIANT_PATH) $(addprefix -D, $(PLATFORM_TAG)) -D__SAM3X8E__ -mthumb -fno-rtti
EXTRA_CPPFLAGS += -nostdlib --param max-inline-insns-single=500 -Dprintf=iprintf $(SYSTEM_FLAGS)

OBJCOPYFLAGS  = -v -Obinary 
TARGET_HEXBIN = $(TARGET_BIN)

# Arduino Due USB PID VID
#
USB_VID   := $(call PARSE_BOARD,$(BOARD_TAG),build.vid)
USB_PID   := $(call PARSE_BOARD,$(BOARD_TAG),build.pid)

USB_FLAGS  = -DUSB_VID=$(USB_VID)
USB_FLAGS += -DUSB_PID=$(USB_PID)
USB_FLAGS += -DUSBCON

# Arduino Due serial 1200 reset
#
USB_TOUCH := $(call PARSE_BOARD,$(BOARD_TAG),upload.protocol)
USB_RESET  = $(UTILITIES_PATH)/serial1200.py

