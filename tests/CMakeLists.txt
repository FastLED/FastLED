cmake_minimum_required(VERSION 3.10)
project(FastLED_Tests)

# Set the path to the FastLED source directory
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

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
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

# Enable testing
enable_testing()

# Add the test executable
add_executable(test_apa102_hd ${CMAKE_CURRENT_SOURCE_DIR}/test_apa102_hd.cpp)
target_link_libraries(test_apa102_hd FastLED)
add_test(NAME APA102HDTest COMMAND test_apa102_hd)
