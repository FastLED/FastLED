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

# Retrieve and print the flags passed from the parent build system
message(STATUS "Using compile flags from parent CMakeLists.txt")
message(STATUS "COMMON_COMPILE_FLAGS: ${COMMON_COMPILE_FLAGS}")
message(STATUS "COMMON_COMPILE_DEFINITIONS: ${COMMON_COMPILE_DEFINITIONS}")

# Verify the directory exists
if(NOT EXISTS ${FASTLED_SOURCE_DIR})
    message(FATAL_ERROR "Error: FASTLED_SOURCE_DIR does not exist! Check directory path.")
endif()

# Include FastLED headers (assumed to be in this directory)
include_directories(${FASTLED_SOURCE_DIR})

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
# Create the main FastLED library from sources
# -----------------------------------------------------------------------------

add_library(fastled STATIC ${FASTLED_SOURCES})

# Apply FastLED library-specific compile flags and definitions
# Note: We apply library-specific flags that disable exceptions/RTTI for embedded compatibility
target_compile_options(fastled PRIVATE 
    # Core warning flags
    -Wall
    -funwind-tables
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O2>
    # FastLED embedded requirements: disable exceptions and RTTI
    -fno-exceptions              # Disable C++ exceptions (embedded requirement)
    -fno-rtti                    # Disable runtime type info (embedded requirement)
    # Dead code elimination
    -ffunction-sections
    -fdata-sections
    # Essential warnings
    -Werror=return-type
    -Werror=missing-declarations
    -Werror=uninitialized
    -Werror=array-bounds
    -Werror=null-dereference
    -Werror=deprecated-declarations
    -Wno-comment
)

# Add GCC-specific flags for FastLED library
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(fastled PRIVATE 
        -Werror=maybe-uninitialized
    )
endif()

# Add C++ specific flags for FastLED library
target_compile_options(fastled PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
    -Werror=suggest-override
    -Werror=non-virtual-dtor
    -Werror=switch-enum
    -Werror=delete-non-virtual-dtor
>)

# Apply FastLED library-specific definitions
target_compile_definitions(fastled PRIVATE 
    FASTLED_FORCE_NAMESPACE=1
    FASTLED_NO_AUTO_NAMESPACE
    FASTLED_STUB_IMPL
    FASTLED_NO_PINMAP
    HAS_HARDWARE_PIN_SUPPORT
    PROGMEM=
    FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8
)

# Ensure full archive linking: force inclusion of all object files
target_link_options(fastled PRIVATE "-Wl,--whole-archive" "-Wl,--no-whole-archive")

# Add Windows debugging libraries for crash handler
if(WIN32)
    target_link_libraries(fastled dbghelp psapi)
endif()

list(LENGTH FASTLED_SOURCES FASTLED_SOURCE_COUNT)
message(STATUS "Created fastled library with ${FASTLED_SOURCE_COUNT} source files")
