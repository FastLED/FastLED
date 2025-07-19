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
# PHASE 2: Build options and validation
# ============================================================================

# Option to disable thin optimizations (archives + LTO) for debugging or compatibility
option(NO_THIN_LTO "Disable thin optimizations (archives and LTO)" OFF)

# Option to disable dead code elimination for debugging or compatibility
option(NO_DEAD_CODE_ELIMINATION "Disable dead code elimination (function/data sections + gc-sections)" OFF)

# Build phase control options
option(NO_LINK "Compile object files but skip linking executables" OFF)
option(NO_BUILD "Skip compilation but perform linking (requires existing object files)" OFF)

# Validate build phase options
if(NO_LINK AND NO_BUILD)
    message(FATAL_ERROR "NO_LINK and NO_BUILD cannot both be enabled simultaneously")
endif()

if(NO_LINK)
    message(STATUS "Build mode: Compile-only (NO_LINK enabled)")
    message(STATUS "  Object files will be created but executables will not be linked")
elseif(NO_BUILD)
    message(STATUS "Build mode: Link-only (NO_BUILD enabled)")
    message(STATUS "  Existing object files will be linked but no compilation will occur")
else()
    message(STATUS "Build mode: Full build (compile + link)")
endif()

# ============================================================================
# PHASE 3: Detection and capability assessment
# ============================================================================
detect_compiler_capabilities()

# Try to find libunwind, but make it optional (from working old version)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBUNWIND QUIET libunwind)
endif()

# If pkg-config didn't work, try manual search
if(NOT LIBUNWIND_FOUND)
    find_path(LIBUNWIND_INCLUDE_DIR 
        NAMES libunwind.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include
    )

    find_library(LIBUNWIND_LIBRARY 
        NAMES unwind
        PATHS /usr/lib /usr/local/lib /opt/homebrew/lib /usr/lib/x86_64-linux-gnu
    )

    find_library(LIBUNWIND_X86_64_LIBRARY 
        NAMES unwind-x86_64
        PATHS /usr/lib /usr/local/lib /opt/homebrew/lib /usr/lib/x86_64-linux-gnu
    )

    # Check if both header and library are found
    if(LIBUNWIND_INCLUDE_DIR AND LIBUNWIND_LIBRARY)
        set(LIBUNWIND_FOUND TRUE)
        if(LIBUNWIND_X86_64_LIBRARY)
            set(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIBRARY} ${LIBUNWIND_X86_64_LIBRARY})
        else()
            set(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIBRARY})
        endif()
        set(LIBUNWIND_INCLUDE_DIRS ${LIBUNWIND_INCLUDE_DIR})
    endif()
endif()

# Set the final flag based on what we found
if(LIBUNWIND_FOUND)
    set(USE_LIBUNWIND TRUE)
    # For x86_64 systems, we need both libunwind and libunwind-x86_64
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(LIBUNWIND_LIBRARIES unwind unwind-x86_64 lzma)
        message(STATUS "LibUnwind found for x86_64: ${LIBUNWIND_LIBRARIES}")
    else()
        # Use pkg-config results if available, otherwise use manual search
        if(PKG_CONFIG_FOUND AND LIBUNWIND_STATIC_LIBRARIES)
            set(LIBUNWIND_LIBRARIES ${LIBUNWIND_STATIC_LIBRARIES})
            set(LIBUNWIND_INCLUDE_DIRS ${LIBUNWIND_STATIC_INCLUDE_DIRS})
            message(STATUS "LibUnwind found via pkg-config: ${LIBUNWIND_LIBRARIES}")
        elseif(LIBUNWIND_LIBRARIES AND LIBUNWIND_INCLUDE_DIRS)
            # Add x86_64 library if available
            if(LIBUNWIND_X86_64_LIBRARY)
                list(APPEND LIBUNWIND_LIBRARIES ${LIBUNWIND_X86_64_LIBRARY})
            endif()
            message(STATUS "LibUnwind found manually: ${LIBUNWIND_LIBRARIES}")
        endif()
    endif()
    message(STATUS "LibUnwind headers: ${LIBUNWIND_INCLUDE_DIRS}")
else()
    set(USE_LIBUNWIND FALSE)
    message(STATUS "LibUnwind not found. Falling back to basic stack traces with execinfo.")
endif()

find_and_configure_dependencies()

# ============================================================================
# PHASE 4: Configure build settings based on build type
# ============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    configure_debug_build()
    set_optimization_level("O0")
    configure_dead_code_elimination()
else()
    configure_release_build()
    set_optimization_level("O2")
    if(NOT NO_THIN_LTO)
        enable_lto()
    endif()
    configure_dead_code_elimination()
endif()

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

# Apply compiler flags using the CompilerFlags module
# This replaces the old manual flag definitions
# For tests, we use test-specific flags which are more permissive but ABI-compatible
apply_test_compiler_flags()      # Sets CMAKE_C_FLAGS and CMAKE_CXX_FLAGS globally for tests

# Note: apply_fastled_cxx_flags() is included in apply_test_compiler_flags()

# Set FASTLED_ALL_SRC=1 for clang builds or when explicitly requested via environment variable
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR DEFINED ENV{FASTLED_ALL_SRC})
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Clang compiler detected: enabling FASTLED_ALL_SRC for unified compilation testing")
    else()
        message(STATUS "FASTLED_ALL_SRC environment variable set: enabling unified compilation testing")
    endif()
    set(FASTLED_ALL_SRC 1)
    add_compile_definitions(FASTLED_ALL_SRC=1)
endif()

# Apply global compile definitions
set(COMMON_COMPILE_DEFINITIONS
    DEBUG
    FASTLED_FORCE_NAMESPACE=1
    FASTLED_USE_JSON_UI=1
    FASTLED_NO_AUTO_NAMESPACE
    FASTLED_TESTING
    ENABLE_CRASH_HANDLER
    FASTLED_STUB_IMPL
    FASTLED_NO_PINMAP
    HAS_HARDWARE_PIN_SUPPORT
    _GLIBCXX_DEBUG
    _GLIBCXX_DEBUG_PEDANTIC
)

# Fix for Microsoft STL version check with older Clang versions on Windows
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMMON_COMPILE_DEFINITIONS _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
endif()

# Apply compile definitions globally
add_compile_definitions(${COMMON_COMPILE_DEFINITIONS})

# ============================================================================
# 🚨 BUILD FLAGS SUMMARY - Show all flags sent to every compilation unit
# ============================================================================

message(STATUS "")
message(STATUS "🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨")
message(STATUS "🚨                                  BUILD FLAGS SUMMARY                                 🚨")
message(STATUS "🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨")
message(STATUS "")

# Display FastLED Library Flags
message(STATUS "📚 FASTLED LIBRARY COMPILATION FLAGS:")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")

# Show all flags that will be applied to FastLED library
string(REPLACE ";" " " COMMON_COMPILE_FLAGS_STR "${COMMON_COMPILE_FLAGS}")
message(STATUS "  📌 C/C++ Common Flags (FastLED Library):")
message(STATUS "     ${COMMON_COMPILE_FLAGS_STR}")
message(STATUS "")

string(REPLACE ";" " " CXX_SPECIFIC_FLAGS_STR "${CXX_SPECIFIC_FLAGS}")
message(STATUS "  📌 C++ Only Flags (FastLED Library):")
message(STATUS "     ${CXX_SPECIFIC_FLAGS_STR}")
message(STATUS "")

string(REPLACE ";" " " COMMON_COMPILE_DEFINITIONS_STR "${COMMON_COMPILE_DEFINITIONS}")
message(STATUS "  📌 Preprocessor Definitions (FastLED Library):")
message(STATUS "     ${COMMON_COMPILE_DEFINITIONS_STR}")
message(STATUS "")

# Show current CMAKE compiler flags (these are applied globally)
message(STATUS "  📌 Final CMAKE_CXX_FLAGS (applied to all C++ files):")
message(STATUS "     ${CMAKE_CXX_FLAGS}")
message(STATUS "")
message(STATUS "  📌 Final CMAKE_C_FLAGS (applied to all C files):")
message(STATUS "     ${CMAKE_C_FLAGS}")
message(STATUS "")

# Display Test Compilation Flags
message(STATUS "🧪 TEST COMPILATION FLAGS:")
string(REPLACE ";" " " UNIT_TEST_COMPILE_FLAGS_STR "${UNIT_TEST_COMPILE_FLAGS}")
message(STATUS "  📌 Unit Test C/C++ Common Flags:")
message(STATUS "     ${UNIT_TEST_COMPILE_FLAGS_STR}")
message(STATUS "")

string(REPLACE ";" " " UNIT_TEST_CXX_FLAGS_STR "${UNIT_TEST_CXX_FLAGS}")
message(STATUS "  📌 Unit Test C++ Only Flags:")
message(STATUS "     ${UNIT_TEST_CXX_FLAGS_STR}")
message(STATUS "")

# Display Linker Flags
message(STATUS "🔗 LINKER FLAGS:")
message(STATUS "  📌 CMAKE_EXE_LINKER_FLAGS (executables):")
message(STATUS "     ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "")
message(STATUS "  📌 CMAKE_SHARED_LINKER_FLAGS (shared libraries):")
message(STATUS "     ${CMAKE_SHARED_LINKER_FLAGS}")
message(STATUS "")
message(STATUS "  📌 CMAKE_MODULE_LINKER_FLAGS (modules):")
message(STATUS "     ${CMAKE_MODULE_LINKER_FLAGS}")
message(STATUS "")

# Additional configuration info
if(USE_LIBUNWIND)
    message(STATUS "🔧 LIBUNWIND CONFIGURATION:")
    message(STATUS "  📌 Libraries: ${LIBUNWIND_LIBRARIES}")
    message(STATUS "  📌 Include Dirs: ${LIBUNWIND_INCLUDE_DIRS}")
    message(STATUS "")
endif()

# Dead code elimination info
if(NOT NO_DEAD_CODE_ELIMINATION)
    string(REPLACE ";" " " DEAD_CODE_ELIMINATION_FLAGS_STR "${DEAD_CODE_ELIMINATION_FLAGS}")
    message(STATUS "🗑️  DEAD CODE ELIMINATION:")
    message(STATUS "  📌 Compiler Flags: ${DEAD_CODE_ELIMINATION_FLAGS_STR}")
    message(STATUS "")
endif()

# ============================================================================
# PHASE 7: Set output directories
# ============================================================================

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin)

# Set binary directory
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin)

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

# Apply static runtime linking on non-Apple platforms (GCC only)
if(NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(fastled PRIVATE -static-libgcc -static-libstdc++)
endif()

# Add C++-specific flags to fastled library
target_compile_options(fastled PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CXX_SPECIFIC_FLAGS}>)

# ============================================================================
# PHASE 9: Create test infrastructure and targets (working old approach)
# ============================================================================

# Enable testing
enable_testing()

# Create a static library for test infrastructure to avoid Windows symbol conflicts
add_library(test_shared_static STATIC
    doctest_main.cpp
)

target_include_directories(test_shared_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# Compiler flags are now applied globally via apply_test_compiler_flags()
target_compile_definitions(test_shared_static PRIVATE ${COMMON_COMPILE_DEFINITIONS})

# Enable debug symbols for the static library on Windows
if(WIN32)
    target_compile_definitions(test_shared_static PRIVATE _DEBUG)
endif()

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

# Create test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # For specific test builds, we know this is the right test
    if(DEFINED SPECIFIC_TEST)
        string(REPLACE "test_" "" CURRENT_TEST_NAME ${TEST_NAME})
        message(STATUS "Processing specific test: ${TEST_NAME}")
    endif()
    
    # ============================================================================
    # 🚨 INDIVIDUAL TEST FLAGS LOGGING
    # ============================================================================
    if(DEFINED SPECIFIC_TEST AND CURRENT_TEST_NAME STREQUAL SPECIFIC_TEST)
        message(STATUS "")
        message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
        message(STATUS "🧪                        SPECIFIC TEST FLAGS: ${TEST_NAME}                        🧪")
        message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
        message(STATUS "")
        message(STATUS "  📁 Source File: ${TEST_SOURCE}")
        message(STATUS "  🎯 Target Name: ${TEST_NAME}")
        message(STATUS "")
        
        # Show the flags that will be applied to this specific test
        message(STATUS "  📌 Test will inherit ALL global C++ flags:")
        message(STATUS "     CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
        message(STATUS "")
        
        message(STATUS "  📌 Test will receive these ADDITIONAL unit test flags:")
        string(REPLACE ";" " " UNIT_TEST_COMPILE_FLAGS_STR "${UNIT_TEST_COMPILE_FLAGS}")
        message(STATUS "     UNIT_TEST_COMPILE_FLAGS: ${UNIT_TEST_COMPILE_FLAGS_STR}")
        message(STATUS "")
        
        message(STATUS "  📌 Test will receive these ADDITIONAL C++ flags:")
        string(REPLACE ";" " " UNIT_TEST_CXX_FLAGS_STR "${UNIT_TEST_CXX_FLAGS}")
        message(STATUS "     UNIT_TEST_CXX_FLAGS: ${UNIT_TEST_CXX_FLAGS_STR}")
        message(STATUS "")
        
        message(STATUS "  📌 Test will receive these compile definitions:")
        string(REPLACE ";" " " COMMON_COMPILE_DEFINITIONS_STR "${COMMON_COMPILE_DEFINITIONS}")
        message(STATUS "     COMMON_COMPILE_DEFINITIONS: ${COMMON_COMPILE_DEFINITIONS_STR}")
        message(STATUS "")
        
        message(STATUS "  📌 Test will be linked with:")
        message(STATUS "     - fastled library")
        message(STATUS "     - test_shared_static library")
        if(WIN32)
            message(STATUS "     - dbghelp psapi (Windows debugging libraries)")
        endif()
        if(USE_LIBUNWIND)
            message(STATUS "     - ${LIBUNWIND_LIBRARIES} (stack unwinding)")
        endif()
        if(NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            message(STATUS "     - static runtime (-static-libgcc -static-libstdc++)")
        endif()
        message(STATUS "")
        
        message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
        message(STATUS "🧪                       END SPECIFIC TEST FLAGS: ${TEST_NAME}                      🧪")
        message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
        message(STATUS "")
    endif()
    # ============================================================================
    
    # Create executable that links to static library
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} fastled test_shared_static)
    
    # Configure Windows-specific settings
    if(WIN32)
        # Add Windows debugging libraries for crash handler
        target_link_libraries(${TEST_NAME} dbghelp psapi)
        
        # Use compatibility layer for Windows debug build settings
        get_windows_debug_build_flags(win_compiler_flags win_linker_flags)
        get_subsystem_flags(subsystem_flags "console")
        
        # Apply the platform-appropriate flags
        if(win_compiler_flags)
            target_compile_options(${TEST_NAME} PRIVATE ${win_compiler_flags})
        endif()
        
        # Combine subsystem and debug linker flags
        set(combined_linker_flags ${subsystem_flags} ${win_linker_flags})
        if(combined_linker_flags)
            string(REPLACE ";" " " combined_linker_flags_str "${combined_linker_flags}")
            set_target_properties(${TEST_NAME} PROPERTIES
                WIN32_EXECUTABLE FALSE
                LINK_FLAGS "${combined_linker_flags_str}")
        else()
            set_target_properties(${TEST_NAME} PROPERTIES
                WIN32_EXECUTABLE FALSE)
        endif()
        
        # Ensure debug information is preserved
        target_compile_definitions(${TEST_NAME} PRIVATE _DEBUG)
    endif()
    
    # Add static linking flags for executables
    if(NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(${TEST_NAME} PRIVATE -static-libgcc -static-libstdc++)
    endif()
    
    # Add libunwind for executables
    if(USE_LIBUNWIND)
        target_link_libraries(${TEST_NAME} ${LIBUNWIND_LIBRARIES})
        target_include_directories(${TEST_NAME} PRIVATE ${LIBUNWIND_INCLUDE_DIRS})
    endif()
    
    # Configure common properties
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    # Compiler flags are now applied globally via apply_test_compiler_flags()
    target_compile_definitions(${TEST_NAME} PRIVATE 
        ${COMMON_COMPILE_DEFINITIONS}
        $<$<BOOL:${USE_LIBUNWIND}>:USE_LIBUNWIND>
    )
    
    # Add test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # ============================================================================
    # 🚨 FINAL INDIVIDUAL TEST LINKER FLAGS LOGGING
    # ============================================================================
    if(DEFINED SPECIFIC_TEST AND CURRENT_TEST_NAME STREQUAL SPECIFIC_TEST)
        message(STATUS "")
        message(STATUS "🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗")
        message(STATUS "🔗                        FINAL LINKER FLAGS: ${TEST_NAME}                         🔗")
        message(STATUS "🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗")
        message(STATUS "")
        
        # Get the actual target properties
        get_target_property(TEST_LINK_FLAGS ${TEST_NAME} LINK_FLAGS)
        if(TEST_LINK_FLAGS)
            message(STATUS "  📌 Target-specific Link Flags:")
            message(STATUS "     ${TEST_LINK_FLAGS}")
        else()
            message(STATUS "  📌 Target-specific Link Flags: (none)")
        endif()
        message(STATUS "")
        
        message(STATUS "  📌 Global Executable Linker Flags:")
        message(STATUS "     CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
        message(STATUS "")
        
        if(WIN32)
            message(STATUS "  📌 Windows-specific Configuration:")
            get_target_property(TEST_WIN32_EXECUTABLE ${TEST_NAME} WIN32_EXECUTABLE)
            message(STATUS "     WIN32_EXECUTABLE: ${TEST_WIN32_EXECUTABLE}")
            message(STATUS "     - Console subsystem enforced")
            message(STATUS "     - Debug symbols included")
        endif()
        
        if(NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            message(STATUS "  📌 Static Runtime Linking:")
            message(STATUS "     -static-libgcc -static-libstdc++")
        endif()
        
        message(STATUS "")
        message(STATUS "🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗")
        message(STATUS "🔗                      END FINAL LINKER FLAGS: ${TEST_NAME}                       🔗")
        message(STATUS "🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗")
        message(STATUS "")
    endif()
    # ============================================================================
endforeach()

# Build summary
get_property(ALL_TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
list(FILTER ALL_TARGETS INCLUDE REGEX "^test_")
list(LENGTH ALL_TARGETS TARGET_COUNT)
if(DEFINED SPECIFIC_TEST)
    message(STATUS "Build summary: Created ${TARGET_COUNT} executable targets (specific test mode)")
else()
    message(STATUS "Build summary: Created ${TARGET_COUNT} executable targets (full build mode)")
endif()

# Add verbose output for tests
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")

# End of modular CMakeLists.txt
# 
# This file has been refactored to use a modular approach with separate 
# CMake modules for different concerns. The old 800+ line monolithic
# file has been replaced with this clean, maintainable structure.
