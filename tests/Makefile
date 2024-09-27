# Compiler and flags
CXX := uv run python -m ziglang c++
CXXFLAGS := -std=c++14 -g -fstack-protector-all -Wno-unknown-pragmas -Wno-unused-parameter -Wno-sign-compare -D_USE_MATH_DEFINES
INCLUDES := -I. -I./src

# Directories
SRC_DIR := src
BUILD_DIR := .build
LIB_NAME := $(BUILD_DIR)/libfastled.a

# Source files
CPP_FILES := $(shell find $(SRC_DIR) -name '*.cpp')
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CPP_FILES))

# Phony targets
.PHONY: all clean

# Default target
all: $(LIB_NAME)

# Rule to create object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Rule to create the static library
$(LIB_NAME): $(OBJ_FILES)
	@mkdir -p $(@D)
	ar rcs $@ $^

# Clean target
clean:
	rm -rf $(BUILD_DIR)

# Print variables for debugging
print-%:
	@echo '$*=$($*)'
