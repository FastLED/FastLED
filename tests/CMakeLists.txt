# Note that we are using the zig compiler as a drop-in replacement for
# gcc. This allows the unit tests to be compiled across different platforms
# without having to worry about the underlying compiler.

cmake_minimum_required(VERSION 3.10)
project(FastLED_Tests)

# Enforce C++17 globally for all targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable parallel compilation
include(ProcessorCount)
ProcessorCount(CPU_COUNT)
if(CPU_COUNT)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${CPU_COUNT})
endif()

# Check if mold linker is available
find_program(MOLD_EXECUTABLE mold)

if(MOLD_EXECUTABLE)
    # Set mold as the default linker
    message(STATUS "Using mold linker: ${MOLD_EXECUTABLE}")
    
    # Add mold linker flags to the common flags
    list(APPEND COMMON_COMPILE_FLAGS "-fuse-ld=mold")
    
    # Set linker flags globally
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=mold")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=mold")
else()
    find_program(LLDLINK_EXECUTABLE lld-link)
    if(LLDLINK_EXECUTABLE)
        message(STATUS "Using lld-link linker: ${LLDLINK_EXECUTABLE}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    else()
        message(STATUS "Neither mold nor lld-link found. Using system default linker.")
    endif()
endif()

# Set build type to Debug
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)

# Output the current build type
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Define common compiler flags and definitions
set(COMMON_COMPILE_FLAGS
    -Wall
    -Wextra 
    #-Wpedantic
    -funwind-tables
    -g3
    -ggdb
    -fno-omit-frame-pointer
    -O0
    -fno-inline
    -Werror=suggest-override
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
    -Werror=non-virtual-dtor
    -Werror=reorder
    -Werror=sign-compare
    -Werror=float-equal
    #-Werror=conversion
    -Werror=switch-enum
    #-Werror=switch-default
    -Werror=unused-parameter
    -Werror=unused-variable
    -Werror=unused-value
    -Werror=cast-align
    -DFASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8
    # ignore Arduino/PlatformIO-specific PROGMEM macro
    -DPROGMEM=
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMMON_COMPILE_FLAGS -Werror=self-assign -Werror=infinite-recursion -Werror=extra-tokens)
endif()


set(UNIT_TEST_COMPILE_FLAGS
    -Wall
    #-Wextra 
    #-Wpedantic
    -funwind-tables
    -g3
    -ggdb
    -fno-omit-frame-pointer
    -O0
    -Werror=suggest-override
    -Werror=return-type
    -Werror=missing-declarations
    #-Werror=redundant-decls
    -Werror=init-self
    #-Werror=missing-field-initializers  
    #-Werror=pointer-arith
    #-Werror=write-strings
    #-Werror=format=2
    #-Werror=implicit-fallthrough
    #-Werror=missing-include-dirs
    -Werror=date-time
    -Werror=non-virtual-dtor
    #-Werror=reorder
    #-Werror=sign-compare
    #-Werror=float-equal
    #-Werror=conversion
    -Werror=switch-enum
    #-Werror=switch-default
    #-Werror=unused-parameter
    #-Werror=unused-variable
    #-Werror=unused-value

    # Not supported in gcc.
    #-Werror=infinite-recursion
    #-v
)

set(COMMON_COMPILE_DEFINITIONS
    DEBUG
    FASTLED_FORCE_NAMESPACE=1
    FASTLED_NO_AUTO_NAMESPACE
    FASTLED_TESTING
    ENABLE_CRASH_HANDLER
    FASTLED_STUB_IMPL
    FASTLED_NO_PINMAP
    HAS_HARDWARE_PIN_SUPPORT
    _GLIBCXX_DEBUG
    _GLIBCXX_DEBUG_PEDANTIC
)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin)

# Set binary directory
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin)

# Set path to FastLED source directory
add_compile_definitions(${COMMON_COMPILE_DEFINITIONS})
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Include FastLED source directory
include_directories(${FASTLED_SOURCE_DIR}/src)

# Delegate source file computation to src/CMakeLists.txt
add_subdirectory(${FASTLED_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/fastled)

if(NOT APPLE)
    target_link_options(fastled PRIVATE -static-libgcc -static-libstdc++)
endif()

# Try to find libunwind, but make it optional
find_package(LibUnwind QUIET)

# Define a variable to check if we should use libunwind
set(USE_LIBUNWIND ${LibUnwind_FOUND})

if(USE_LIBUNWIND)
    message(STATUS "LibUnwind found. Using it for better stack traces.")
else()
    message(STATUS "LibUnwind not found. Falling back to basic stack traces.")
endif()

# Enable testing
enable_testing()

# Find all test source files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

# Find test executables (only actual test executables, not libraries)
file(GLOB TEST_BINARIES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_*${CMAKE_EXECUTABLE_SUFFIX}")

# Process source files
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} fastled)
    if(USE_LIBUNWIND)
        target_link_libraries(${TEST_NAME} ${LIBUNWIND_LIBRARIES})
    endif()
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    # Add static linking flags and debug flags for test executables
    if(NOT APPLE)
        target_link_options(${TEST_NAME} PRIVATE -static-libgcc -static-libstdc++)
    endif()
    target_compile_options(${TEST_NAME} PRIVATE ${UNIT_TEST_COMPILE_FLAGS})
    target_compile_definitions(${TEST_NAME} PRIVATE 
        ${COMMON_COMPILE_DEFINITIONS}
        $<$<BOOL:${USE_LIBUNWIND}>:USE_LIBUNWIND>
    )
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# Process remaining binaries (those without corresponding source files)
option(CLEAN_ORPHANED_BINARIES "Remove orphaned test binaries" ON)
if(CLEAN_ORPHANED_BINARIES)
    foreach(ORPHANED_BINARY ${TEST_BINARIES})
        get_filename_component(BINARY_NAME ${ORPHANED_BINARY} NAME_WE)
        get_filename_component(BINARY_DIR ${ORPHANED_BINARY} DIRECTORY)
        get_filename_component(PARENT_DIR ${BINARY_DIR} DIRECTORY)
        get_filename_component(GRANDPARENT_DIR ${PARENT_DIR} DIRECTORY)
        set(CORRESPONDING_SOURCE "${GRANDPARENT_DIR}/${BINARY_NAME}.cpp")
        if(NOT EXISTS "${CORRESPONDING_SOURCE}")
            message(STATUS "Found orphaned binary without source: ${ORPHANED_BINARY}")
            file(REMOVE "${ORPHANED_BINARY}")
            message(STATUS "Deleted orphaned binary: ${ORPHANED_BINARY}")
        endif()
    endforeach()
endif()

# Add verbose output for tests
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
