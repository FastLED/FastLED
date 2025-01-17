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

# Linker flags
set(COMMON_LINK_FLAGS
    -sALLOW_MEMORY_GROWTH=0
    -sDISABLE_EXCEPTION_CATCHING=1
    -sDISABLE_EXCEPTION_THROWING=0
    --bind
    -DUSE_OFFSET_CONVERTER=0
    -sINITIAL_MEMORY=134217728
    -fuse-ld=lld
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']
    -sEXPORTED_FUNCTIONS=['_malloc','_free','_extern_setup','_extern_loop','_fastled_declare_files']
    --no-entry
    -sMODULARIZE=1
    -sEXPORT_NAME=fastled
    -o fastled.js
)

# Debug-specific flags
if(BUILD_MODE STREQUAL "DEBUG")
    list(APPEND COMMON_COMPILE_FLAGS
        -g3
        -fsanitize=address
        -fsanitize=undefined
    )
    list(APPEND COMMON_LINK_FLAGS
        -gsource-map
        --emit-symbol-map
        -sSTACK_OVERFLOW_CHECK=2
        -ASSERTIONS=1
    )
endif()

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

# Create core FastLED library
add_library(FastLED STATIC ${FASTLED_SOURCES})

# Set properties for the FastLED library
set_target_properties(FastLED PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
target_compile_options(FastLED PRIVATE ${COMMON_COMPILE_FLAGS})

# Create the WASM executable and link FastLED
add_executable(fastled)
target_link_libraries(fastled PRIVATE FastLED)

# Set properties for the fastled target
set_target_properties(fastled PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_compile_options(fastled PRIVATE ${COMMON_COMPILE_FLAGS})
target_link_options(fastled PRIVATE ${COMMON_LINK_FLAGS})

# Debug output for source categorization
message(STATUS "Current Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")

# Remove platform-specific files that might cause issues
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")

# Try to find libunwind, but make it optional
find_package(LibUnwind QUIET)

# Define a variable to check if we should use libunwind
set(USE_LIBUNWIND ${LibUnwind_FOUND})

if(USE_LIBUNWIND)
    message(STATUS "LibUnwind found. Using it for better stack traces.")
else()
    message(STATUS "LibUnwind not found. Falling back to basic stack traces.")
endif()

# Add verbose output for tests
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
