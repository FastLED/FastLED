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

# All source build system removed - using standard individual file compilation
message(STATUS "Using individual file compilation mode with automatic source globbing")

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

# -----------------------------------------------------------------------------
# Create the main FastLED library from all globbed sources
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
message(STATUS "Created fastled library with ${FASTLED_SOURCE_COUNT} source files")
