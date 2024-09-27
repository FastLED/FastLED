cmake_minimum_required(VERSION 3.10)
project(FastLED_Tests)

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
    "${FASTLED_SOURCE_DIR}/src/*.c"
)

# Remove platform-specific files that might cause issues
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")

# Create FastLED library
add_library(FastLED STATIC ${FASTLED_SOURCES})

# Set C++ standard
set_target_properties(FastLED PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Enable testing
enable_testing()

# Find all test files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

# Add each test file as an executable
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} FastLED)
    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
