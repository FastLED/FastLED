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
# Last update: Jun 21, 2013 release 54



# Arduino specifics
# ----------------------------------
# Automatic 0023 or 1.x.x selection based on version.txt
#
ifneq ($(shell grep 1. $(ARDUINO_PATH)/lib/version.txt),)
    include $(MAKEFILE_PATH)/Arduino1.mk	
else
    include $(MAKEFILE_PATH)/Arduino23.mk	
endif

