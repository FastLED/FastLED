QUIET ?= 1
ifeq ($(QUIET),1)
  AD=@
else
  AD=
endif

TARGET=LinuxDemo

SRC = LinuxDemo.cpp


OBJDIR = objs
OBJS = $(SRC:%.cpp=$(OBJDIR)/%.o)
DEPENDENCIES = $(OBJS:%.o=%.d)

INCLUDE_PATHS=  -I. -I/usr/local/include/FastLED

CC      = g++
CFLAGS += -std=gnu++11  -MD -MP -g -ffunction-sections -fdata-sections $(INCLUDE_PATHS)
LIBS	= -L/usr/local/lib -lfastled

.PHONY: clean

all: $(TARGET)

build: $(TARGET)

clean:
	rm -rf $(OBJDIR) LinuxDemo

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.o: %.cpp
	@echo "Compiling $<"
	$(AD)$(CC) -c $(CFLAGS) -o $@ $<

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)"
	$(CC) -g  -o $@ $(OBJS) $(LIBS)

-include $(DEPENDENCIES)
