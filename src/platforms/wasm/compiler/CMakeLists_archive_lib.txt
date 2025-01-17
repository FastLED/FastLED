cmake_minimum_required(VERSION 3.10)
project(FastLED_WASM)

# Set Emscripten as the compiler
set(CMAKE_C_COMPILER emcc)
set(CMAKE_CXX_COMPILER em++)

# Get build mode from environment, default to QUICK
if(NOT DEFINED ENV{BUILD_MODE})
    set(BUILD_MODE "QUICK")
else()
    set(BUILD_MODE $ENV{BUILD_MODE})
endif()

# Set optimization level based on build mode
if(BUILD_MODE STREQUAL "QUICK")
    set(OPT_LEVEL "-O1")
elseif(BUILD_MODE STREQUAL "DEBUG")
    set(OPT_LEVEL "-O0")
else() # RELEASE
    set(OPT_LEVEL "-Oz")
endif()

# Common compiler flags
set(COMMON_COMPILE_FLAGS
    -DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50
    -DFASTLED_FORCE_NAMESPACE=1
    -DFASTLED_USE_PROGMEM=0
    -DDISABLE_EXCEPTION_CATCHING=1
    -fno-exceptions
    -fno-rtti
    -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0
    -std=gnu++17
    -fpermissive
    -Wno-constant-logical-operand
    -Wnon-c-typedef-for-linkage
    -Werror=bad-function-cast
    -Werror=cast-function-type
    ${OPT_LEVEL}
)

# Set FastLED source directory
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fastled/src)

message(STATUS "FastLED source directory: ${FASTLED_SOURCE_DIR}")

# Include directories
include_directories(${FASTLED_SOURCE_DIR})

# Find all FastLED source files
file(GLOB_RECURSE FASTLED_SOURCES 
    "${FASTLED_SOURCE_DIR}/*.cpp"
)

message(STATUS "FastLED sources: ${FASTLED_SOURCES}")

# Remove platform-specific files that might cause issues
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")

# Create core FastLED library archive
add_library(FastLED STATIC ${FASTLED_SOURCES})

# Set properties for the FastLED library
set_target_properties(FastLED PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Apply compile options directly (no private linking, all symbols visible)
target_compile_options(FastLED PUBLIC ${COMMON_COMPILE_FLAGS})
