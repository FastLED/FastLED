# Note that we are using the zig compiler as a drop in replacement for
# gcc. This allows the unit tests to be compiled across different platforms
# without having to worry about the underlying compiler.

cmake_minimum_required(VERSION 3.10)
project(FastLED_Tests)

# Set default build type to Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# Output the current build type
message(STATUS "Current build type: ${CMAKE_BUILD_TYPE}")

# Function to wrap Zig command
function(use_zig_as_cpp)
    find_program(UV_EXECUTABLE uv)
    if(NOT UV_EXECUTABLE)
        message(FATAL_ERROR "uv not found. Please install uv.")
    endif()

    find_program(PYTHON_EXECUTABLE python)
    if(NOT PYTHON_EXECUTABLE)
        message(FATAL_ERROR "Python not found. Please install Python.")
    endif()

    # uv run python -m ziglang c++
    set(CMAKE_CXX_COMPILER_LAUNCHER ${UV_EXECUTABLE} run ${PYTHON_EXECUTABLE} -m ziglang c++)
    set(CMAKE_C_COMPILER_LAUNCHER ${UV_EXECUTABLE} run ${PYTHON_EXECUTABLE} -m ziglang cc)
    set(CMAKE_CXX_COMPILER c++)
    set(CMAKE_C_COMPILER cc)
endfunction()

# Use Zig as the C++ compiler
use_zig_as_cpp()

# Set the path to the FastLED source directory
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin)

# Set binary directory
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin)

# Include FastLED source directory
include_directories(${FASTLED_SOURCE_DIR}/src)

# Find all FastLED source files
file(GLOB_RECURSE FASTLED_SOURCES 
    "${FASTLED_SOURCE_DIR}/src/*.cpp"
)

# Remove platform-specific files that might cause issues
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")

# Create FastLED library
add_library(FastLED STATIC ${FASTLED_SOURCES})

# Set C++ standard and static linking for FastLED
set_target_properties(FastLED PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)


# Add static linking flags and debug flags for FastLED
if(NOT APPLE)
    target_link_options(FastLED PRIVATE -static-libgcc -static-libstdc++)
endif()

# Add custom build flags for FastLED
target_compile_options(FastLED PRIVATE 
    -Wall
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG -DFASTLED_STUB_IMPL>
    $<$<CONFIG:Release>:-O3>
)

# Enable testing
enable_testing()

# Find all test source files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

# Find all test binaries
file(GLOB TEST_BINARIES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_*${CMAKE_EXECUTABLE_SUFFIX}")

# Process source files
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} FastLED)
    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    # Add static linking flags and debug flags for test executables
    if(NOT APPLE)
        target_link_options(${TEST_NAME} PRIVATE -static-libgcc -static-libstdc++)
    endif()
    target_compile_options(${TEST_NAME} PRIVATE 
        -Wall
        $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
        $<$<CONFIG:Release>:-O3>
    )
    
    # Create a custom command to build the test only if source or FastLED library changes
    add_custom_command(
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEST_NAME}
        COMMAND ${CMAKE_COMMAND} --build . --target ${TEST_NAME}
        DEPENDS ${TEST_SOURCE} FastLED
        COMMENT "Building ${TEST_NAME}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    # Create a custom target that depends on the custom command
    add_custom_target(
        ${TEST_NAME}_target ALL
        DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEST_NAME}
    )
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# Process remaining binaries (those without corresponding source files)
foreach(ORPHANED_BINARY ${TEST_BINARIES})
    message(STATUS "Found orphaned binary without source: ${ORPHANED_BINARY}")
    file(REMOVE ${ORPHANED_BINARY})
    message(STATUS "Deleted orphaned binary: ${ORPHANED_BINARY}")
endforeach()

# Add verbose output for tests
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
