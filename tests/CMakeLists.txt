# FastLED Test Suite - Modular CMake Build System
# Note that we are using the zig compiler as a drop-in replacement for
# gcc. This allows the unit tests to be compiled across different platforms
# without having to worry about the underlying compiler.

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
# PHASE 6: Apply compiler flags directly (based on working old version)
# ============================================================================

# Set build type to Debug
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Full debugging settings (enables step-through debugging)
set(FAST_DEBUG_OPT -O0)  # No optimization for accurate debugging

# Generate both debug formats: DWARF for GDB and CodeView for stack traces
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # On Windows with Clang: generate both DWARF (for GDB) and CodeView (for stack traces)
    set(FAST_DEBUG_INFO -g3 -gdwarf-4)  # DWARF for GDB compatibility + full debug info
    message(STATUS "Using dual debug format: DWARF for GDB + CodeView for stack traces")
else()
    # Non-Windows or non-Clang: standard DWARF debug info
    set(FAST_DEBUG_INFO -g3)  # Full debug info including macros
    message(STATUS "Using DWARF debug format")
endif()

# Windows symbol files for stack traces
if(WIN32)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang supports CodeView debug format for Windows
        set(FAST_DEBUG_INFO ${FAST_DEBUG_INFO} -gcodeview)
        message(STATUS "Adding CodeView debug info for Windows stack traces (Clang)")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC doesn't support CodeView, but DWARF format works well for stack traces
        message(STATUS "Using DWARF debug format for Windows stack traces (GCC)")
    endif()
endif()

set(FAST_FRAME_POINTER -fno-omit-frame-pointer)

# Get dead code elimination flags
if(NOT NO_DEAD_CODE_ELIMINATION)
    message(STATUS "Enabling dead code elimination for smaller binaries")
    set(DEAD_CODE_ELIMINATION_FLAGS "-ffunction-sections" "-fdata-sections")
else()
    message(STATUS "Dead code elimination disabled")
    set(DEAD_CODE_ELIMINATION_FLAGS "")
endif()

# Define common compiler flags and definitions (from working old version)
set(COMMON_COMPILE_FLAGS
    -Wall
    -Wextra 
    -funwind-tables
    ${FAST_DEBUG_INFO}
    ${FAST_FRAME_POINTER}
    ${FAST_DEBUG_OPT}
    ${DEAD_CODE_ELIMINATION_FLAGS}
    # 🚨 NOTE: Allowing exceptions and RTTI for easier testing
    # Production builds can disable these with -fno-exceptions -fno-rtti if needed
    -Werror=return-type
    -Werror=missing-declarations
    -Werror=redundant-decls
    -Werror=init-self
    -Werror=missing-field-initializers  
    -Werror=pointer-arith
    -Werror=write-strings
    -Werror=format=2
    -Werror=implicit-fallthrough
    -Werror=missing-include-dirs
    -Werror=date-time
    -Werror=unused-parameter
    -Werror=unused-variable
    -Werror=unused-value
    -Werror=cast-align
    -Werror=misleading-indentation
    -Werror=uninitialized
    -Werror=array-bounds
    -Werror=strict-overflow=2
    -Werror=null-dereference
    -Werror=format-security
    -Werror=unused-function
    -Werror=deprecated-declarations
    -DFASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8
    -Wno-comment
    # ignore Arduino/PlatformIO-specific PROGMEM macro
    -DPROGMEM=
)

# Add GCC-specific warning flags
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND COMMON_COMPILE_FLAGS 
        -Werror=maybe-uninitialized
        -Werror=logical-op
        -Werror=class-memaccess
    )
endif()

# C++-specific compiler flags
set(CXX_SPECIFIC_FLAGS
    -Werror=suggest-override
    -Werror=non-virtual-dtor
    -Werror=reorder
    -Werror=sign-compare
    -Werror=float-equal
    -Werror=mismatched-tags
    -Werror=switch-enum
    -Werror=delete-non-virtual-dtor
)

# Add GCC-specific C++ warning flags
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND CXX_SPECIFIC_FLAGS 
        -Werror=redundant-move
    )
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND CXX_SPECIFIC_FLAGS -Werror=self-assign -Werror=infinite-recursion -Werror=extra-tokens -Werror=unused-private-field -Wglobal-constructors -Werror=global-constructors)
endif()

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

# Apply compiler flags globally 
foreach(flag ${COMMON_COMPILE_FLAGS})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endforeach()

# Test-specific flags (more permissive)
set(UNIT_TEST_COMPILE_FLAGS
    -Wall
    -funwind-tables
    ${FAST_DEBUG_INFO}
    ${FAST_FRAME_POINTER}
    ${FAST_DEBUG_OPT}
    ${DEAD_CODE_ELIMINATION_FLAGS}
    -Werror=return-type
    -Werror=missing-declarations
    -Werror=init-self
    -Werror=date-time
    -Werror=uninitialized
    -Werror=array-bounds
    -Werror=null-dereference
    -Werror=deprecated-declarations
    -Wno-comment
    # 🚨 HACK: More permissive flags for tests (common test code patterns)
    -Wno-unused-parameter      # Test functions often have unused params
    -Wno-unused-variable       # Test code often has temporary variables
    -Wno-unused-value          # Test assertions may appear to not use values
)

# Add GCC-specific unit test warning flags
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND UNIT_TEST_COMPILE_FLAGS 
        -Werror=maybe-uninitialized
    )
endif()

set(UNIT_TEST_CXX_FLAGS
    -Werror=suggest-override
    -Werror=non-virtual-dtor
    -Werror=switch-enum
    -Werror=delete-non-virtual-dtor
)

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
target_compile_options(test_shared_static PRIVATE ${UNIT_TEST_COMPILE_FLAGS})
target_compile_options(test_shared_static PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${UNIT_TEST_CXX_FLAGS}>)
target_compile_definitions(test_shared_static PRIVATE ${COMMON_COMPILE_DEFINITIONS})

# Enable debug symbols for the static library on Windows
if(WIN32)
    target_compile_definitions(test_shared_static PRIVATE _DEBUG)
endif()

# Find all test source files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

# Create test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # If SPECIFIC_TEST is defined, only create that test target
    if(DEFINED SPECIFIC_TEST)
        # Check if this is the specific test we want to build
        string(REPLACE "test_" "" CURRENT_TEST_NAME ${TEST_NAME})
        if(NOT CURRENT_TEST_NAME STREQUAL SPECIFIC_TEST)
            # Skip this test, it's not the one we want
            continue()
        endif()
        message(STATUS "Building specific test: ${TEST_NAME}")
    endif()
    
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
    target_compile_options(${TEST_NAME} PRIVATE ${UNIT_TEST_COMPILE_FLAGS})
    target_compile_options(${TEST_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${UNIT_TEST_CXX_FLAGS}>)
    target_compile_definitions(${TEST_NAME} PRIVATE 
        ${COMMON_COMPILE_DEFINITIONS}
        $<$<BOOL:${USE_LIBUNWIND}>:USE_LIBUNWIND>
    )
    
    # Add test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
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
