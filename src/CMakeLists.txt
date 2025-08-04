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

# Check if we should use unified compilation mode (FASTLED_ALL_SRC=1)
# Responds to directives from test system or environment variables
# Does NOT make compiler-specific decisions - that's handled by the test system
if(NOT DEFINED FASTLED_ALL_SRC)
    # Only set default if not already specified by parent or environment
    option(FASTLED_ALL_SRC "Enable unified compilation mode" OFF)
    message(STATUS "FASTLED_ALL_SRC not specified: using default OFF (individual file compilation)")
else()
    # Respect the value passed from parent CMake or environment
    message(STATUS "FASTLED_ALL_SRC specified by parent/environment: ${FASTLED_ALL_SRC}")
endif()

if(FASTLED_ALL_SRC)
    message(STATUS "FASTLED_ALL_SRC=ON: Using unified compilation mode")
    
    # === Get all the source files ===
    file(GLOB_RECURSE ALL_CPP_FILES "${FASTLED_SOURCE_DIR}/*.cpp")
    
    # Exclude platform-specific files (e.g. esp, arm, avr)
    list(FILTER ALL_CPP_FILES EXCLUDE REGEX ".*esp.*")
    list(FILTER ALL_CPP_FILES EXCLUDE REGEX ".*arm.*")
    list(FILTER ALL_CPP_FILES EXCLUDE REGEX ".*avr.*")
    
    list(LENGTH ALL_CPP_FILES CPP_FILE_COUNT)
    message(STATUS "Found ${CPP_FILE_COUNT} .cpp files for unified compilation")
    
    # Organize files by subdirectory for detailed reporting
    set(ROOT_FILES "")
    set(FL_FILES "")
    set(FX_FILES "")
    set(SENSORS_FILES "")
    set(PLATFORMS_FILES "")
    set(THIRD_PARTY_FILES "")
    set(OTHER_FILES "")
    
    foreach(cpp_file ${ALL_CPP_FILES})
        file(RELATIVE_PATH relative_path ${FASTLED_SOURCE_DIR} ${cpp_file})
        
        if(relative_path MATCHES "^fl/")
            list(APPEND FL_FILES ${relative_path})
        elseif(relative_path MATCHES "^fx/")
            list(APPEND FX_FILES ${relative_path})
        elseif(relative_path MATCHES "^sensors/")
            list(APPEND SENSORS_FILES ${relative_path})
        elseif(relative_path MATCHES "^platforms/")
            list(APPEND PLATFORMS_FILES ${relative_path})
        elseif(relative_path MATCHES "^third_party/")
            list(APPEND THIRD_PARTY_FILES ${relative_path})
        elseif(NOT relative_path MATCHES "/")
            list(APPEND ROOT_FILES ${relative_path})
        else()
            list(APPEND OTHER_FILES ${relative_path})
        endif()
    endforeach()
    
    # Report files by category
    message(STATUS "=== UNIFIED COMPILATION FILE BREAKDOWN ===")
    
    list(LENGTH ROOT_FILES ROOT_COUNT)
    if(ROOT_COUNT GREATER 0)
        message(STATUS "Root src/ files (${ROOT_COUNT}):")
        foreach(file ${ROOT_FILES})
            message(STATUS "  ${file}")
        endforeach()
    endif()
    
    list(LENGTH FL_FILES FL_COUNT)
    if(FL_COUNT GREATER 0)
        message(STATUS "FL library files (${FL_COUNT}):")
        foreach(file ${FL_FILES})
            message(STATUS "  ${file}")
        endforeach()
    endif()
    
    list(LENGTH FX_FILES FX_COUNT)
    if(FX_COUNT GREATER 0)
        message(STATUS "FX library files (${FX_COUNT}):")
        foreach(file ${FX_FILES})
            message(STATUS "  ${file}")
        endforeach()
    endif()
    
    list(LENGTH SENSORS_FILES SENSORS_COUNT)
    if(SENSORS_COUNT GREATER 0)
        message(STATUS "Sensors library files (${SENSORS_COUNT}):")
        foreach(file ${SENSORS_FILES})
            message(STATUS "  ${file}")
        endforeach()
    endif()
    
    list(LENGTH PLATFORMS_FILES PLATFORMS_COUNT)
    if(PLATFORMS_COUNT GREATER 0)
        message(STATUS "Platforms library files (${PLATFORMS_COUNT}):")
        foreach(file ${PLATFORMS_FILES})
            message(STATUS "  ${file}")
        endforeach()
    endif()
    
    list(LENGTH THIRD_PARTY_FILES THIRD_PARTY_COUNT)
    if(THIRD_PARTY_COUNT GREATER 0)
        message(STATUS "Third party library files (${THIRD_PARTY_COUNT}):")
        foreach(file ${THIRD_PARTY_FILES})
            message(STATUS "  ${file}")
        endforeach()
    endif()
    
    list(LENGTH OTHER_FILES OTHER_COUNT)
    if(OTHER_COUNT GREATER 0)
        message(STATUS "Other files (${OTHER_COUNT}):")
        foreach(file ${OTHER_FILES})
            message(STATUS "  ${file}")
        endforeach()
    endif()
    
    message(STATUS "=== TOTAL: ${CPP_FILE_COUNT} files combined into unified compilation ===")
    
    # Generate unified source file that includes all .cpp files
    set(UNIFIED_SOURCE_FILE "${CMAKE_CURRENT_BINARY_DIR}/fastled_unified.cpp")
    
    # Create content for unified source file
    set(UNIFIED_CONTENT "// Auto-generated unified compilation file\n")
    set(UNIFIED_CONTENT "${UNIFIED_CONTENT}// This file includes all FastLED source files for unified compilation\n")
    set(UNIFIED_CONTENT "${UNIFIED_CONTENT}// Generated from ${CPP_FILE_COUNT} .cpp files\n\n")
    
    foreach(cpp_file ${ALL_CPP_FILES})
        # Convert absolute path to relative path from FASTLED_SOURCE_DIR
        file(RELATIVE_PATH relative_path ${FASTLED_SOURCE_DIR} ${cpp_file})
        set(UNIFIED_CONTENT "${UNIFIED_CONTENT}#include \"${relative_path}\"\n")
    endforeach()
    
    # Write the unified source file
    file(WRITE ${UNIFIED_SOURCE_FILE} ${UNIFIED_CONTENT})
    
    # Use only the unified source file for compilation
    set(FASTLED_SOURCES ${UNIFIED_SOURCE_FILE})
    
    message(STATUS "Generated unified source file: ${UNIFIED_SOURCE_FILE}")
    
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
if(FASTLED_ALL_SRC)
    message(STATUS "Created fastled library with unified compilation (${CPP_FILE_COUNT} .cpp files combined into ${FASTLED_SOURCE_COUNT} compilation unit)")
else()
    message(STATUS "Created fastled library with ${FASTLED_SOURCE_COUNT} individual source files")
endif()
