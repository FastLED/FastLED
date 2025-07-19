# FastLED Test Suite - Modular CMake Build System
# Note that we are using the zig compiler as a drop-in replacement for
# gcc. This allows the unit tests to be compiled across different platforms
# without having to worry about the underlying compiler.

message(STATUS "=== CMAKE STARTING ===")
if(DEFINED SPECIFIC_TEST)
    message(STATUS "🎯 SPECIFIC_TEST IS SET TO: ${SPECIFIC_TEST}")
else()
    message(STATUS "🌐 SPECIFIC_TEST IS NOT DEFINED - BUILDING ALL TESTS")
endif()
message(STATUS "======================")

cmake_minimum_required(VERSION 3.10)
project(FastLED_Tests)

# ============================================================================
# PHASE 1: Include all modules
# ============================================================================
include(cmake/BuildOptions.cmake)
include(cmake/CompilerDetection.cmake)
include(cmake/CompilerFlags.cmake) 
include(cmake/LinkerCompatibility.cmake)
include(cmake/DebugSettings.cmake)
include(cmake/OptimizationSettings.cmake)
include(cmake/DependencyManagement.cmake)
include(cmake/ParallelBuild.cmake)
include(cmake/TargetCreation.cmake)
include(cmake/TestConfiguration.cmake)

# Enforce C++17 globally for all targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# PHASE 2: Build options and validation (REFACTORED)
# ============================================================================
configure_build_options()

# ============================================================================
# PHASE 3: Detection and capability assessment
# ============================================================================
detect_compiler_capabilities()

# Dependency detection is now handled by the DependencyManagement module
find_and_configure_dependencies()

# ============================================================================
# PHASE 4: Configure build settings based on build type (REFACTORED)
# ============================================================================
configure_build_type_settings()

# ============================================================================
# PHASE 5: Configure build performance and parallelization
# ============================================================================
optimize_build_performance()

# ============================================================================
# PHASE 6: Apply compiler flags using modules (REFACTORED)
# ============================================================================

# Set build type to Debug
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Apply compiler flags and definitions using the CompilerFlags module
apply_test_compiler_flags()              # Sets CMAKE_C_FLAGS and CMAKE_CXX_FLAGS globally
apply_test_compile_definitions()         # Sets all test-specific compile definitions

# ============================================================================
# PHASE 6: Display build configuration summary (REFACTORED)
# ============================================================================
display_build_configuration_summary()

# ============================================================================
# PHASE 7: Configure build output directories (REFACTORED)
# ============================================================================
configure_build_output_directories()

# ============================================================================
# PHASE 8: Include FastLED source directory and create targets
# ============================================================================

# 🚨 DEBUG: Check if SPECIFIC_TEST variable is being set
if(DEFINED SPECIFIC_TEST)
    message(STATUS "DEBUG: SPECIFIC_TEST VARIABLE IS SET: '${SPECIFIC_TEST}'")
else()
    message(STATUS "DEBUG: SPECIFIC_TEST VARIABLE IS NOT SET - BUILDING ALL TESTS")
endif()

# Set path to FastLED source directory
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Include FastLED source directory
include_directories(${FASTLED_SOURCE_DIR}/src)

# Delegate source file computation to src/CMakeLists.txt
add_subdirectory(${FASTLED_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/fastled)

# Platform-specific static runtime linking is now handled by the TargetCreation module
# via apply_static_runtime_linking() function calls

# ============================================================================
# PHASE 9: Create test infrastructure and targets (working old approach)
# ============================================================================

# Test infrastructure and testing setup is now handled by modular functions:
# - enable_testing() is called by configure_ctest()
# - test_shared_static library is created by create_test_infrastructure()

# Determine which test source files to process
if(DEFINED SPECIFIC_TEST)
    # Only process the specific test file
    set(SPECIFIC_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test_${SPECIFIC_TEST}.cpp")
    message(STATUS "🔍 Looking for specific test file: ${SPECIFIC_TEST_SOURCE}")
    if(EXISTS ${SPECIFIC_TEST_SOURCE})
        set(TEST_SOURCES ${SPECIFIC_TEST_SOURCE})
        message(STATUS "✅ Building ONLY specific test: test_${SPECIFIC_TEST}")
    else()
        message(FATAL_ERROR "❌ Specific test file not found: ${SPECIFIC_TEST_SOURCE}")
    endif()
else()
    # Find all test source files for full build
    file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
    list(LENGTH TEST_SOURCES TEST_COUNT)
    message(STATUS "🌐 Building ALL tests - found ${TEST_COUNT} test files")
endif()

# ============================================================================
# PHASE 8: Create test targets using modular system (REFACTORED)
# ============================================================================

# Configure CTest system
configure_ctest()

# Create test infrastructure library
create_test_infrastructure()

# Create test executables using modular approach
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # For specific test builds, we know this is the right test
    if(DEFINED SPECIFIC_TEST)
        string(REPLACE "test_" "" CURRENT_TEST_NAME ${TEST_NAME})
        message(STATUS "Processing specific test: ${TEST_NAME}")
    endif()
    
    # ============================================================================
    # 🚨 MODULAR TEST CREATION LOGGING
    # ============================================================================
    if(DEFINED SPECIFIC_TEST AND CURRENT_TEST_NAME STREQUAL SPECIFIC_TEST)
        message(STATUS "")
        message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
        message(STATUS "🧪                        MODULAR TEST CREATION: ${TEST_NAME}                       🧪")
        message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
        message(STATUS "")
        message(STATUS "  📁 Source File: ${TEST_SOURCE}")
        message(STATUS "  🎯 Target Name: ${TEST_NAME}")
        message(STATUS "")
        
        message(STATUS "  🔧 Modular Target Creation:")
        message(STATUS "     - Using create_test_executable() from TargetCreation module")
        message(STATUS "     - Automatic Windows configuration via configure_windows_executable()")
        message(STATUS "     - Standard test settings via apply_test_settings()")
        message(STATUS "     - Unit test flags via apply_unit_test_flags()")
        message(STATUS "     - CTest registration via register_test_executable()")
        message(STATUS "")
        
        message(STATUS "  📌 Modular system will automatically provide:")
        message(STATUS "     - FastLED library linking")
        message(STATUS "     - Test infrastructure (test_shared_static) linking")
        message(STATUS "     - C++17 standard compliance")
        message(STATUS "     - Platform-specific debugging libraries (Windows: dbghelp, psapi)")
        message(STATUS "     - Stack unwinding support (if available)")
        message(STATUS "     - Static runtime linking (GNU on non-Apple platforms)")
        message(STATUS "     - Consistent compile definitions and flags")
        message(STATUS "")
        
        message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
        message(STATUS "🧪                       END MODULAR TEST CREATION: ${TEST_NAME}                    🧪")
        message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
        message(STATUS "")
    endif()
    # ============================================================================
    
    # Create test executable using modular system
    create_test_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # Register with CTest
    register_test_executable(${TEST_NAME})
    
    # Display detailed linker flags for specific test builds (debugging)
    if(DEFINED SPECIFIC_TEST AND CURRENT_TEST_NAME STREQUAL SPECIFIC_TEST)
        display_target_linker_flags(${TEST_NAME})
    endif()
endforeach()

# ============================================================================
# PHASE 9: Display build summary (REFACTORED)
# ============================================================================
display_build_summary()

# End of modular CMakeLists.txt
# 
# This file has been refactored to use a modular approach with separate 
# CMake modules for different concerns. The old 800+ line monolithic
# file has been replaced with this clean, maintainable structure.
