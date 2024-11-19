# Note that we are using the zig compiler as a drop in replacement for
# gcc. This allows the unit tests to be compiled across different platforms
# without having to worry about the underlying compiler.

cmake_minimum_required(VERSION 3.10)
project(FastLED_Tests)

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
    message(STATUS "Mold linker not found. Using system default linker.")
endif()

# Set build type to Debug
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)

# Output the current build type
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Define common compiler flags and definitions
set(COMMON_COMPILE_FLAGS
    -Wall
    -funwind-tables
    -g3
    -ggdb
    -fno-omit-frame-pointer
    -O0
    -fno-inline
)

set(COMMON_COMPILE_DEFINITIONS
    DEBUG
    FASTLED_FORCE_NAMESPACE
    FASTLED_TESTING
    ENABLE_CRASH_HANDLER
    FASTLED_STUB_IMPL
    FASTLED_NO_PINMAP
    HAS_HARDWARE_PIN_SUPPORT
    _GLIBCXX_DEBUG
    _GLIBCXX_DEBUG_PEDANTIC
)

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
    "${FASTLED_SOURCE_DIR}/src/fx/*.cpp"
    "${FASTLED_SOURCE_DIR}/src/platforms/*.cpp"
    "${FASTLED_SOURCE_DIR}/src/lib8tion/*.cpp"
    "${FASTLED_SOURCE_DIR}/src/fl/*.cpp"
)

message(STATUS "Found FastLED sources: ${FASTLED_SOURCES}")

# Group sources by directory
set(FX_SOURCES "")
set(PLATFORMS_SOURCES "")
set(LIB8TION_SOURCES "")
set(FL_SOURCES "")
set(OTHER_SOURCES "")

# Categorize each source file
foreach(SOURCE ${FASTLED_SOURCES})
    if(SOURCE MATCHES "/fx/")
        list(APPEND FX_SOURCES ${SOURCE})
    elseif(SOURCE MATCHES "/platforms/")
        list(APPEND PLATFORMS_SOURCES ${SOURCE})
    elseif(SOURCE MATCHES "/lib8tion/")
        list(APPEND LIB8TION_SOURCES ${SOURCE})
    elseif(SOURCE MATCHES "/fl/")
        list(APPEND FL_SOURCES ${SOURCE})
    else()
        list(APPEND OTHER_SOURCES ${SOURCE})
    endif()
endforeach()

# Debug output for source categorization
message(STATUS "FX Sources: ${FX_SOURCES}")
message(STATUS "Platforms Sources: ${PLATFORMS_SOURCES}")
message(STATUS "Lib8tion Sources: ${LIB8TION_SOURCES}")
message(STATUS "FL Sources: ${FL_SOURCES}")
message(STATUS "Other Sources: ${OTHER_SOURCES}")

# Create core library with essential components
file(GLOB CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp")

add_library(FastLED_core STATIC ${CORE_SOURCES})
set_target_properties(FastLED_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
target_compile_options(FastLED_core PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_definitions(FastLED_core PRIVATE ${COMMON_COMPILE_DEFINITIONS})
if(NOT APPLE)
    target_link_options(FastLED_core PRIVATE -static-libgcc -static-libstdc++)
endif()

# Create a list of available libraries
set(FASTLED_LIBS FastLED_core)
if(FX_SOURCES)
    add_library(FastLED_fx STATIC ${FX_SOURCES})
    target_link_libraries(FastLED_fx PUBLIC FastLED_core)
    list(APPEND FASTLED_LIBS FastLED_fx)
endif()
if(PLATFORMS_SOURCES)
    add_library(FastLED_platforms STATIC ${PLATFORMS_SOURCES})
    target_link_libraries(FastLED_platforms PUBLIC FastLED_core)
    list(APPEND FASTLED_LIBS FastLED_platforms)
endif()
if(LIB8TION_SOURCES)
    add_library(FastLED_lib8tion STATIC ${LIB8TION_SOURCES})
    target_link_libraries(FastLED_lib8tion PUBLIC FastLED_core)
    list(APPEND FASTLED_LIBS FastLED_lib8tion)
endif()
if(FL_SOURCES)
    add_library(FastLED_fl STATIC ${FL_SOURCES})
    target_link_libraries(FastLED_fl PUBLIC FastLED_core)
    list(APPEND FASTLED_LIBS FastLED_fl)
endif()

# Set properties for all created libraries
foreach(LIB ${FASTLED_LIBS})
    set_target_properties(${LIB} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(${LIB} PRIVATE ${COMMON_COMPILE_FLAGS})
    target_compile_definitions(${LIB} PRIVATE ${COMMON_COMPILE_DEFINITIONS})
    if(NOT APPLE)
        target_link_options(${LIB} PRIVATE -static-libgcc -static-libstdc++)
    endif()
endforeach()

# Try to find libunwind, but make it optional
find_package(LibUnwind QUIET)

# Define a variable to check if we should use libunwind
set(USE_LIBUNWIND ${LibUnwind_FOUND})

if(USE_LIBUNWIND)
    message(STATUS "LibUnwind found. Using it for better stack traces.")
else()
    message(STATUS "LibUnwind not found. Falling back to basic stack traces.")
endif()

# Remove platform-specific files that might cause issues
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")


# Create FastLEDTest with OTHER_SOURCES
add_library(FastLEDTest STATIC ${OTHER_SOURCES})

# Link with the sub-libraries if they exist
target_link_libraries(FastLEDTest PUBLIC ${FASTLED_LIBS})

# Set C++ standard and shared library properties for FastLEDTest
set_target_properties(FastLEDTest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Add static linking flags and debug flags for FastLEDTest
if(NOT APPLE)
    target_link_options(FastLEDTest PRIVATE -static-libgcc -static-libstdc++)
endif()

# Add custom build flags for FastLEDTest
target_compile_options(FastLEDTest PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_definitions(FastLEDTest PRIVATE ${COMMON_COMPILE_DEFINITIONS})

if(USE_LIBUNWIND)
    target_link_libraries(FastLEDTest PRIVATE ${LIBUNWIND_LIBRARIES})
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
    target_link_libraries(${TEST_NAME} FastLEDTest)
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
    target_compile_options(${TEST_NAME} PRIVATE ${COMMON_COMPILE_FLAGS})
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
