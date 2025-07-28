# ExampleCompileTest.cmake
# Ultra-fast compilation testing of Arduino .ino examples

# Function to discover all .ino files in the examples directory
function(discover_ino_examples OUTPUT_VAR)
    set(EXAMPLES_DIR "${CMAKE_SOURCE_DIR}/../examples")
    
    # Find all .ino files recursively
    file(GLOB_RECURSE INO_FILES "${EXAMPLES_DIR}/*/*.ino")
    
    # Filter out temporary or test files
    set(FILTERED_FILES)
    foreach(INO_FILE ${INO_FILES})
        get_filename_component(INO_NAME ${INO_FILE} NAME)
        get_filename_component(INO_DIR ${INO_FILE} DIRECTORY)
        get_filename_component(DIR_NAME ${INO_DIR} NAME)
        
        # Skip temporary test files and some problematic examples
        if(NOT INO_NAME MATCHES "^temp_" AND 
           NOT DIR_NAME MATCHES "^temp_" AND
           NOT DIR_NAME STREQUAL "TclColors" AND
           NOT DIR_NAME STREQUAL "WS2812_ADAFRUIT" AND
           NOT DIR_NAME STREQUAL "NeoPixelBusTest" AND
           NOT DIR_NAME STREQUAL "JsonFetch" AND
           NOT DIR_NAME STREQUAL "VariantVisitor" AND
           NOT DIR_NAME STREQUAL "FestivalStick" AND  # Compiler ICE in curr.h:437
           NOT DIR_NAME STREQUAL "OctoWS2811Demo" AND  # Requires external OctoWS2811 library
           NOT DIR_NAME STREQUAL "ParallelOutputDemo")  # Requires platform-specific WS2811_PORTDC
            
            # Check if specific examples are requested
            if(DEFINED FASTLED_SPECIFIC_EXAMPLES AND FASTLED_SPECIFIC_EXAMPLES)
                # Convert semicolon-separated list to CMake list
                string(REPLACE ";" "|" SPECIFIC_EXAMPLES_REGEX "${FASTLED_SPECIFIC_EXAMPLES}")
                if(DIR_NAME MATCHES "^(${SPECIFIC_EXAMPLES_REGEX})$")
                    list(APPEND FILTERED_FILES ${INO_FILE})
                endif()
            else()
                # No specific examples requested, include all valid examples
                list(APPEND FILTERED_FILES ${INO_FILE})
            endif()
        endif()
    endforeach()
    
    set(${OUTPUT_VAR} ${FILTERED_FILES} PARENT_SCOPE)
    
    list(LENGTH FILTERED_FILES COUNT)
    message(STATUS "Discovered ${COUNT} .ino examples for compilation testing")
endfunction()

# Function to detect if a .ino file includes FastLED
function(detect_fastled_inclusion INO_FILE OUTPUT_VAR)
    file(READ ${INO_FILE} FILE_CONTENT)
    
    # Check for FastLED inclusion patterns (simplified regex)
    if(FILE_CONTENT MATCHES "#include.*FastLED\\.h" OR
       FILE_CONTENT MATCHES "#include.*fastled\\.h")
        set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
        message(STATUS "FastLED detected in: ${INO_FILE}")
    else()
        set(${OUTPUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Function to detect if a .ino file has function overrides
function(detect_function_overrides INO_FILE OUTPUT_VAR)
    file(READ ${INO_FILE} FILE_CONTENT)
    
    # Check for override patterns
    if(FILE_CONTENT MATCHES "FASTLED_.*_OVERRIDE")
        set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
        message(STATUS "Function override detected in: ${INO_FILE}")
    else()
        set(${OUTPUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Function to prepare .ino file for direct compilation
function(prepare_ino_for_compilation INO_FILE COMPILE_DIR HAS_FASTLED OUTPUT_VAR)
    get_filename_component(INO_NAME ${INO_FILE} NAME_WE)
    get_filename_component(INO_DIR ${INO_FILE} DIRECTORY)
    get_filename_component(DIR_NAME ${INO_DIR} NAME)
    
    # Check if this example has function overrides
    detect_function_overrides(${INO_FILE} HAS_OVERRIDES)
    
    # Create unique target name to avoid conflicts
    set(TARGET_NAME "example_${DIR_NAME}_${INO_NAME}")
    
    # Create a wrapper .cpp file that includes compatibility header first
    set(WRAPPER_CPP_FILE "${COMPILE_DIR}/${TARGET_NAME}_wrapper.cpp")
    
    # Generate wrapper content that includes compatibility fixes first
    set(WRAPPER_CONTENT "// Generated wrapper for ${INO_FILE}\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}// This ensures arduino_compat.h is included first for MSVC compatibility\n\n")
    
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}#include \"arduino_compat.h\"  // MSVC compatibility fixes\n\n")
    
    if(HAS_FASTLED AND NOT HAS_OVERRIDES)
        set(WRAPPER_CONTENT "${WRAPPER_CONTENT}// FastLED-enabled example\n")
        set(WRAPPER_CONTENT "${WRAPPER_CONTENT}#include \"FastLED.h\"\n\n")
    elseif(HAS_OVERRIDES)
        set(WRAPPER_CONTENT "${WRAPPER_CONTENT}// FastLED example with function overrides - include .ino first\n")
        set(WRAPPER_CONTENT "${WRAPPER_CONTENT}// The .ino file defines overrides before including FastLED.h\n\n")
    else()
        set(WRAPPER_CONTENT "${WRAPPER_CONTENT}// Basic Arduino example (no FastLED)\n\n")
    endif()
    
    # Include the original .ino file
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}// Include original .ino file\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}#include \"${INO_FILE}\"\n\n")
    
    # Add main function if needed
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}// Main function for linkage testing\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}#ifndef ARDUINO_MAIN_DEFINED\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}int main() {\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}    setup();\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}    loop();\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}    return 0;\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}}\n")
    set(WRAPPER_CONTENT "${WRAPPER_CONTENT}#endif\n")
    
    # Write the wrapper file
    file(WRITE ${WRAPPER_CPP_FILE} ${WRAPPER_CONTENT})
    
    # Store metadata for debugging
    set(INO_INFO_FILE "${COMPILE_DIR}/${TARGET_NAME}.info")
    set(INFO_CONTENT "INO_FILE=${INO_FILE}\n")
    set(INFO_CONTENT "${INFO_CONTENT}HAS_FASTLED=${HAS_FASTLED}\n")
    set(INFO_CONTENT "${INFO_CONTENT}TARGET_NAME=${TARGET_NAME}\n")
    set(INFO_CONTENT "${INFO_CONTENT}WRAPPER_FILE=${WRAPPER_CPP_FILE}\n")
    file(WRITE ${INO_INFO_FILE} ${INFO_CONTENT})
    
    # Return the wrapper .cpp file for compilation
    set(${OUTPUT_VAR} ${WRAPPER_CPP_FILE} PARENT_SCOPE)
    
    message(STATUS "Generated wrapper: ${TARGET_NAME} (FastLED: ${HAS_FASTLED})")
endfunction()

# Function to create precompiled header
function(create_fastled_pch STUB_DIR)
    set(PCH_HEADER "${STUB_DIR}/fastled_pch.h")
    set(PCH_FILE "${STUB_DIR}/fastled_pch.pch")
    
    # Generate PCH header content with proper paths
    set(PCH_CONTENT "#pragma once\n\n")
    set(PCH_CONTENT "${PCH_CONTENT}// FastLED Precompiled Header for Example Compilation Testing\n")
    set(PCH_CONTENT "${PCH_CONTENT}// This file contains commonly used FastLED headers for faster compilation\n\n")
    
    set(PCH_CONTENT "${PCH_CONTENT}// MSVC Compatibility Layer (must be first)\n")
    set(PCH_CONTENT "${PCH_CONTENT}#include \"arduino_compat.h\"\n\n")
    
    set(PCH_CONTENT "${PCH_CONTENT}// Core FastLED headers\n")
    set(PCH_CONTENT "${PCH_CONTENT}#include \"FastLED.h\"\n\n")
    
    set(PCH_CONTENT "${PCH_CONTENT}// Arduino compatibility\n")
    set(PCH_CONTENT "${PCH_CONTENT}#include \"platforms/wasm/compiler/Arduino.h\"\n\n")
    
    set(PCH_CONTENT "${PCH_CONTENT}// Common FastLED namespace usage\n")
    set(PCH_CONTENT "${PCH_CONTENT}FASTLED_USING_NAMESPACE\n")
    
    # Write PCH header
    file(WRITE ${PCH_HEADER} ${PCH_CONTENT})
    
    # Store PCH paths for use by compilation targets
    set(FASTLED_PCH_HEADER ${PCH_HEADER} PARENT_SCOPE)
    set(FASTLED_PCH_FILE ${PCH_FILE} PARENT_SCOPE)
    
    message(STATUS "FastLED PCH prepared: ${PCH_HEADER}")
    message(STATUS "PCH will be compiled by build targets for maximum speed")
endfunction()

# Function to configure example compilation test
function(configure_example_compile_test)
    message(STATUS "=== Configuring Example Compilation Test ===")
    
    # Set up compilation directory
    set(COMPILE_DIR "${CMAKE_CURRENT_BINARY_DIR}/example_compile_direct")
    file(MAKE_DIRECTORY ${COMPILE_DIR})
    
    # Create precompiled header for FastLED examples
    create_fastled_pch(${COMPILE_DIR})
    
    # Discover examples
    discover_ino_examples(INO_FILES)
    
    if(NOT INO_FILES)
        message(WARNING "No .ino examples found for compilation testing")
        return()
    endif()
    
    # Process each example for direct compilation
    set(COMPILE_INO_FILES)
    set(FASTLED_INO_FILES)
    set(NON_FASTLED_INO_FILES)
    set(FASTLED_WRAPPER_FILES)
    set(NON_FASTLED_WRAPPER_FILES)
    set(FASTLED_COUNT 0)
    set(NON_FASTLED_COUNT 0)
    
    foreach(INO_FILE ${INO_FILES})
        # Detect FastLED inclusion
        detect_fastled_inclusion(${INO_FILE} HAS_FASTLED)
        
        # Prepare wrapper file for compilation
        prepare_ino_for_compilation(${INO_FILE} ${COMPILE_DIR} ${HAS_FASTLED} WRAPPER_FILE)
        list(APPEND COMPILE_INO_FILES ${WRAPPER_FILE})
        
        # Separate FastLED and non-FastLED files for different compilation settings
        if(HAS_FASTLED)
            list(APPEND FASTLED_INO_FILES ${INO_FILE})
            list(APPEND FASTLED_WRAPPER_FILES ${WRAPPER_FILE})
            math(EXPR FASTLED_COUNT "${FASTLED_COUNT} + 1")
        else()
            list(APPEND NON_FASTLED_INO_FILES ${INO_FILE})
            list(APPEND NON_FASTLED_WRAPPER_FILES ${WRAPPER_FILE})
            math(EXPR NON_FASTLED_COUNT "${NON_FASTLED_COUNT} + 1")
        endif()
    endforeach()
    
    # Store results for use by other functions
    set(EXAMPLE_COMPILE_INO_FILES ${COMPILE_INO_FILES} PARENT_SCOPE)
    set(EXAMPLE_FASTLED_INO_FILES ${FASTLED_INO_FILES} PARENT_SCOPE)
    set(EXAMPLE_NON_FASTLED_INO_FILES ${NON_FASTLED_INO_FILES} PARENT_SCOPE)
    set(EXAMPLE_FASTLED_WRAPPER_FILES ${FASTLED_WRAPPER_FILES} PARENT_SCOPE)
    set(EXAMPLE_NON_FASTLED_WRAPPER_FILES ${NON_FASTLED_WRAPPER_FILES} PARENT_SCOPE)
    set(EXAMPLE_COMPILE_DIR ${COMPILE_DIR} PARENT_SCOPE)
    
    list(LENGTH INO_FILES TOTAL_COUNT)
    message(STATUS "Example compilation prepared:")
    message(STATUS "  Total examples: ${TOTAL_COUNT}")
    message(STATUS "  With FastLED: ${FASTLED_COUNT}")
    message(STATUS "  Without FastLED: ${NON_FASTLED_COUNT}")
    message(STATUS "  Compile directory: ${COMPILE_DIR}")
    message(STATUS "  Method: Direct .ino compilation (no stub files needed)")
endfunction()

# Function to create example compilation test target using direct .ino compilation
function(create_example_compile_test_target)
    if(NOT EXAMPLE_FASTLED_WRAPPER_FILES AND NOT EXAMPLE_NON_FASTLED_WRAPPER_FILES)
        message(WARNING "No example wrapper files available - skipping test target creation")
        return()
    endif()
    
    # Determine compiler and flags based on CMake generator and compiler
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        # Visual Studio generator always uses MSVC-style flags regardless of compiler detection
        message(STATUS "Visual Studio generator detected - using MSVC-compatible flags")
        set(USE_CLANG_CL FALSE)
        set(EXAMPLE_COMPILER ${CMAKE_CXX_COMPILER})
    elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        find_program(CLANG_CL_EXECUTABLE clang-cl PATHS "C:/Program Files/LLVM/bin")
        
        if(CLANG_CL_EXECUTABLE)
            message(STATUS "Found Clang-CL: ${CLANG_CL_EXECUTABLE}")
            message(STATUS "Using Clang-CL for example compilation (FastLED + MSVC compatibility)")
            set(EXAMPLE_COMPILER ${CLANG_CL_EXECUTABLE})
            set(USE_CLANG_CL TRUE)
        else()
            message(WARNING "Clang-CL not found - falling back to MSVC with compatibility layer")
            set(EXAMPLE_COMPILER ${CMAKE_CXX_COMPILER})
            set(USE_CLANG_CL FALSE)
        endif()
    else()
        set(EXAMPLE_COMPILER ${CMAKE_CXX_COMPILER})
        set(USE_CLANG_CL FALSE)
    endif()
    
    # Set up compiler flags based on the selected compiler
    if(USE_CLANG_CL AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        # Clang-CL flags (MSVC-compatible interface with Clang backend)
        set(FAST_COMPILE_FLAGS
            /Od                   # No optimization for fastest compilation
            /GR-                  # Disable RTTI
            /EHs-c-               # Disable exception handling
            -Wno-unused-parameter # Suppress unused parameter warnings
            -Wno-unused-variable  # Suppress unused variable warnings
            -Wno-unused-function  # Suppress unused function warnings
            -Wno-unknown-pragmas  # Suppress unknown pragma warnings
            -Wno-deprecated-declarations  # Suppress deprecated warnings
            -Wno-macro-redefined  # Suppress macro redefinition warnings
        )
        message(STATUS "Using Clang-CL compilation flags for FastLED compatibility")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC" OR CMAKE_GENERATOR MATCHES "Visual Studio")
        set(FAST_COMPILE_FLAGS
            /Od                   # No optimization for fastest compilation
            /GR-                  # Disable RTTI
            /EHs-c-               # Disable exception handling
            /wd4100               # Suppress unused parameter warnings
            /wd4101               # Suppress unused variable warnings
            /wd4505               # Suppress unused function warnings
            /wd4005               # Suppress macro redefinition warnings
            /wd4068               # Suppress unknown pragma warnings
            /wd4477               # Suppress format string warnings
            /wd4267               # Suppress conversion warnings
            /wd4091               # Suppress typedef warnings
        )
        message(STATUS "Using MSVC compilation flags with enhanced compatibility for FastLED")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(FAST_COMPILE_FLAGS
            -O0                   # No optimization for fastest compilation
            -g0                   # No debug symbols
            -fno-rtti             # Disable RTTI
            -fno-exceptions       # Disable exception handling
            -pipe                 # Use pipes instead of temp files
            -ffast-math           # Fast math operations
            -fno-stack-protector  # Disable stack protection for speed
            -fno-unwind-tables    # Disable unwind tables
            -Wno-unused-parameter # Suppress unused parameter warnings
            -Wno-unused-variable  # Suppress unused variable warnings
            -Wno-unused-function  # Suppress unused function warnings
            -Wno-unknown-pragmas  # Suppress unknown pragma warnings
            -Wno-deprecated-declarations  # Suppress deprecated warnings
            -Wno-everything       # Suppress all warnings for maximum speed
        )
        message(STATUS "Using Clang ultra-fast compilation flags for example compilation")
    else()
        set(FAST_COMPILE_FLAGS
            -O0                   # No optimization for fastest compilation
            -g0                   # No debug symbols
            -fno-rtti             # Disable RTTI
            -fno-exceptions       # Disable exception handling
            -pipe                 # Use pipes instead of temp files
            -ffast-math           # Fast math operations
            -fno-stack-protector  # Disable stack protection for speed
            -fno-unwind-tables    # Disable unwind tables
            -Wno-unused-parameter # Suppress unused parameter warnings
            -Wno-unused-variable  # Suppress unused variable warnings
            -Wno-unused-function  # Suppress unused function warnings
            -w                    # Suppress all warnings for maximum speed
        )
        message(STATUS "Using GCC ultra-fast compilation flags for example compilation")
    endif()
    
    # Create compilation test target with FastLED examples
    if(EXAMPLE_FASTLED_WRAPPER_FILES)
        add_library(example_compile_fastled_objects OBJECT ${EXAMPLE_FASTLED_WRAPPER_FILES})
        
        # Set compiler for this target if using Clang-CL
        if(USE_CLANG_CL)
            set_target_properties(example_compile_fastled_objects PROPERTIES
                CXX_COMPILER_LAUNCHER ""
            )
            # Use CMAKE_CXX_COMPILER_LAUNCHER to override compiler
            set_property(TARGET example_compile_fastled_objects PROPERTY 
                CXX_COMPILER "${EXAMPLE_COMPILER}")
        endif()
        
        # Apply fast compilation flags
        target_compile_options(example_compile_fastled_objects PRIVATE ${FAST_COMPILE_FLAGS})
        
        # Set include directories - arduino_compat.h available in cmake directory
        target_include_directories(example_compile_fastled_objects PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake         # arduino_compat.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../src        # FastLED headers
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/platforms/wasm/compiler
            ${CMAKE_CURRENT_SOURCE_DIR}/../examples   # Original .ino files
            ${EXAMPLE_COMPILE_DIR}
        )
        
        # Enable precompiled headers for FastLED examples (major speed boost)
        if(FASTLED_PCH_HEADER AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
            target_precompile_headers(example_compile_fastled_objects PRIVATE ${FASTLED_PCH_HEADER})
            message(STATUS "PCH enabled for FastLED examples: ${FASTLED_PCH_HEADER}")
        else()
            message(STATUS "PCH not available (CMake < 3.16 or no PCH header)")
        endif()
        
        # Define preprocessor symbols for compatibility
        target_compile_definitions(example_compile_fastled_objects PRIVATE
            FASTLED_EXAMPLE_COMPILE_TEST=1
            ARDUINO_ARCH_WASM=1
            FASTLED_WASM=1
            ARDUINO=10819
            FASTLED_STUB_IMPL=1
        )
        
        # Note: We're now compiling wrapper .cpp files, not .ino files directly
        
        list(LENGTH EXAMPLE_FASTLED_WRAPPER_FILES FASTLED_COUNT)
        message(STATUS "Created FastLED example compilation target: example_compile_fastled_objects (${FASTLED_COUNT} files)")
    endif()
    
    # Create compilation test target with non-FastLED examples
    if(EXAMPLE_NON_FASTLED_WRAPPER_FILES)
        add_library(example_compile_basic_objects OBJECT ${EXAMPLE_NON_FASTLED_WRAPPER_FILES})
        
        # Set compiler for this target if using Clang-CL
        if(USE_CLANG_CL)
            set_target_properties(example_compile_basic_objects PROPERTIES
                CXX_COMPILER_LAUNCHER ""
            )
            # Use CMAKE_CXX_COMPILER_LAUNCHER to override compiler
            set_property(TARGET example_compile_basic_objects PROPERTY 
                CXX_COMPILER "${EXAMPLE_COMPILER}")
        endif()
        
        # Apply fast compilation flags
        target_compile_options(example_compile_basic_objects PRIVATE ${FAST_COMPILE_FLAGS})
        
        # Set include directories - arduino_compat.h available in cmake directory
        target_include_directories(example_compile_basic_objects PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake         # arduino_compat.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/platforms/wasm/compiler
            ${CMAKE_CURRENT_SOURCE_DIR}/../examples   # Original .ino files
            ${EXAMPLE_COMPILE_DIR}
        )
        
        # Define preprocessor symbols for compatibility
        target_compile_definitions(example_compile_basic_objects PRIVATE
            FASTLED_EXAMPLE_COMPILE_TEST=1
            ARDUINO_ARCH_WASM=1
            ARDUINO=10819
        )
        
        # Note: We're now compiling wrapper .cpp files, not .ino files directly
        
        list(LENGTH EXAMPLE_NON_FASTLED_WRAPPER_FILES NON_FASTLED_COUNT)
        message(STATUS "Created basic example compilation target: example_compile_basic_objects (${NON_FASTLED_COUNT} files)")
    endif()
    
    if(USE_CLANG_CL)
        message(STATUS "Direct .ino compilation targets created with Clang-CL (FastLED optimized)!")
    else()
        message(STATUS "Direct .ino compilation targets created with enhanced MSVC compatibility!")
    endif()
endfunction()

# Function to create example compilation test executable
function(create_example_compilation_test_executable)
    # Skip creating the test executable here - it will be handled by the standard test discovery
    # This function is a placeholder for future example compilation infrastructure
    message(STATUS "Example compilation test will be handled by standard test discovery")
endfunction()
