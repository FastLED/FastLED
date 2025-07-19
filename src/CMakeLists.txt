# -----------------------------------------------------------------------------
# Efficiently compiles the libfastled.a archive to link against.
# Optionally, you can copy the header tree to a specified include path.
# -----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5)

# Set FastLED source directory (this is where the FastLED sources live)
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "FASTLED_SOURCE_DIR: ${FASTLED_SOURCE_DIR}")

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    message(STATUS "CMAKE_CXX_STANDARD not defined. Setting C++ standard to 17.")
else()
    message(STATUS "CMAKE_CXX_STANDARD already defined as: ${CMAKE_CXX_STANDARD}")
endif()

# Retrieve and print the flags passed from the parent (e.g. tests/CMakeLists.txt)
message(STATUS "Using compile flags from parent CMakeLists.txt")
message(STATUS "COMMON_COMPILE_FLAGS: ${COMMON_COMPILE_FLAGS}")
message(STATUS "COMMON_COMPILE_DEFINITIONS: ${COMMON_COMPILE_DEFINITIONS}")

# Verify the directory exists
if(NOT EXISTS ${FASTLED_SOURCE_DIR})
    message(FATAL_ERROR "Error: FASTLED_SOURCE_DIR does not exist! Check directory path.")
endif()

# Include FastLED headers (assumed to be in this directory)
include_directories(${FASTLED_SOURCE_DIR})

# Check if we should use unified compilation mode (FASTLED_ALL_SRC=1)
option(FASTLED_ALL_SRC "Enable unified compilation mode" ON)

if(FASTLED_ALL_SRC)
    message(STATUS "FASTLED_ALL_SRC=ON: Using unified compilation mode")
    
    # === Get all the source files ===
    file(GLOB_RECURSE ALL_CPP_FILES "${FASTLED_SOURCE_DIR}/*.cpp")
    message(STATUS "Found ${CMAKE_CURRENT_LIST_LENGTH} .cpp files for unified compilation")
    
    # Exclude platform-specific files (e.g. esp, arm, avr)
    list(FILTER ALL_CPP_FILES EXCLUDE REGEX ".*esp.*")
    list(FILTER ALL_CPP_FILES EXCLUDE REGEX ".*arm.*")
    list(FILTER ALL_CPP_FILES EXCLUDE REGEX ".*avr.*")
    
    # Generate unified source file that includes all .cpp files
    set(UNIFIED_SOURCE_FILE "${CMAKE_CURRENT_BINARY_DIR}/fastled_unified.cpp")
    
    # Create content for unified source file
    set(UNIFIED_CONTENT "// Auto-generated unified compilation file\n")
    set(UNIFIED_CONTENT "${UNIFIED_CONTENT}// This file includes all FastLED source files for unified compilation\n\n")
    
    foreach(cpp_file ${ALL_CPP_FILES})
        # Convert absolute path to relative path from FASTLED_SOURCE_DIR
        file(RELATIVE_PATH relative_path ${FASTLED_SOURCE_DIR} ${cpp_file})
        set(UNIFIED_CONTENT "${UNIFIED_CONTENT}#include \"${relative_path}\"\n")
    endforeach()
    
    # Write the unified source file
    file(WRITE ${UNIFIED_SOURCE_FILE} ${UNIFIED_CONTENT})
    
    # Use only the unified source file for compilation
    set(FASTLED_SOURCES ${UNIFIED_SOURCE_FILE})
    
    list(LENGTH ALL_CPP_FILES CPP_FILE_COUNT)
    message(STATUS "Generated unified source file including ${CPP_FILE_COUNT} .cpp files")
    
else()
    message(STATUS "FASTLED_ALL_SRC=OFF: Using individual file compilation mode")
    
    # === Get all the source files ===
    file(GLOB_RECURSE FASTLED_SOURCES "${FASTLED_SOURCE_DIR}/*.c*")
    message(STATUS "Found source files: ${FASTLED_SOURCES}")

    if(FASTLED_SOURCES STREQUAL "")
        message(FATAL_ERROR "Error: No source files found in ${FASTLED_SOURCE_DIR}!")
    endif()

    # Exclude platform-specific files (e.g. esp, arm, avr)
    list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
    list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
    list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")
endif()

# -----------------------------------------------------------------------------
# Create the main FastLED library from sources (unified or individual)
# -----------------------------------------------------------------------------

add_library(fastled STATIC ${FASTLED_SOURCES})

# Apply compile flags and definitions
target_compile_options(fastled PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_options(fastled PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CXX_SPECIFIC_FLAGS}>)
target_compile_definitions(fastled PRIVATE ${COMMON_COMPILE_DEFINITIONS})

# Ensure full archive linking: force inclusion of all object files
target_link_options(fastled PRIVATE "-Wl,--whole-archive" "-Wl,--no-whole-archive")

# Add Windows debugging libraries for crash handler
if(WIN32)
    target_link_libraries(fastled dbghelp psapi)
endif()

list(LENGTH FASTLED_SOURCES FASTLED_SOURCE_COUNT)
if(FASTLED_ALL_SRC)
    message(STATUS "Created fastled library with unified compilation (${CPP_FILE_COUNT} .cpp files combined into ${FASTLED_SOURCE_COUNT} compilation unit)")
else()
    message(STATUS "Created fastled library with ${FASTLED_SOURCE_COUNT} individual source files")
endif()
