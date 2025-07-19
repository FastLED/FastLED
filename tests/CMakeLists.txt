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
include(cmake/TestSourceDiscovery.cmake)

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
# PHASE 8: Include FastLED source and create test targets (REFACTORED)
# ============================================================================

# Setup FastLED source directory (handled by TestSourceDiscovery module)
setup_fastled_source_directory()

# Configure CTest system and create test infrastructure
configure_ctest()
create_test_infrastructure()

# Process test targets using modular approach (handled by TestSourceDiscovery module)
process_test_targets()

# ============================================================================
# PHASE 9: Display build summary (REFACTORED)
# ============================================================================
display_build_summary()

# End of modular CMakeLists.txt
# 
# This file has been refactored to use a modular approach with separate 
# CMake modules for different concerns. The old 800+ line monolithic
# file has been replaced with this clean, maintainable structure.
