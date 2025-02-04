# -----------------------------------------------------------------------------
# Efficiently compiles the libfastled.a archive to link against.
# Optionally, you can copy the header tree to a specified include path.
# -----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5)

# Set FastLED source directory (this is where the FastLED sources live)
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "FASTLED_SOURCE_DIR: ${FASTLED_SOURCE_DIR}")

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    message(STATUS "CMAKE_CXX_STANDARD not defined. Setting C++ standard to 17.")
else()
    message(STATUS "CMAKE_CXX_STANDARD already defined as: ${CMAKE_CXX_STANDARD}")
endif()

# Retrieve and print the flags passed from the parent (e.g. tests/CMakeLists.txt)
message(STATUS "Using compile flags from parent CMakeLists.txt")
message(STATUS "COMMON_COMPILE_FLAGS: ${COMMON_COMPILE_FLAGS}")
message(STATUS "COMMON_COMPILE_DEFINITIONS: ${COMMON_COMPILE_DEFINITIONS}")

# Verify the directory exists
if(NOT EXISTS ${FASTLED_SOURCE_DIR})
    message(FATAL_ERROR "Error: FASTLED_SOURCE_DIR does not exist! Check directory path.")
endif()

# Include FastLED headers (assumed to be in this directory)
include_directories(${FASTLED_SOURCE_DIR})

# === Step 1: Get all the source files ===
file(GLOB_RECURSE FASTLED_SOURCES "${FASTLED_SOURCE_DIR}/*.c*")
message(STATUS "Found source files: ${FASTLED_SOURCES}")

if(FASTLED_SOURCES STREQUAL "")
    message(FATAL_ERROR "Error: No source files found in ${FASTLED_SOURCE_DIR}!")
endif()

# Exclude platform-specific files (e.g. esp, arm, avr)
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")


# -----------------------------------------------------------------------------
# Partition helper: create object libraries and add their names to a global list
# -----------------------------------------------------------------------------

# Initialize a global list to store the names of the created object libraries.
set(FASTLED_OBJECT_LIBS "")

# Using a macro (instead of a function) ensures we are in the global scope.
macro(add_partitioned_library lib_name source_regex)
    set(matched_sources "")
    foreach(source IN LISTS FASTLED_SOURCES)
        if(source MATCHES "${source_regex}")
            list(APPEND matched_sources "${source}")
        endif()
    endforeach()

    if(matched_sources)
        # Remove the matched sources from the global list so they won't be processed again.
        foreach(source IN LISTS matched_sources)
            list(REMOVE_ITEM FASTLED_SOURCES "${source}")
        endforeach()

        # Create an object library from the matched sources.
        add_library(${lib_name} OBJECT ${matched_sources})
        target_compile_options(${lib_name} PRIVATE ${COMMON_COMPILE_FLAGS})
        target_compile_definitions(${lib_name} PRIVATE ${COMMON_COMPILE_DEFINITIONS})
        message(STATUS "Created ${lib_name} with sources: ${matched_sources}")

        # Append the library name to the global list.
        list(APPEND FASTLED_OBJECT_LIBS ${lib_name})
    else()
        message(WARNING "No sources found for library ${lib_name} using filter: ${source_regex}")
    endif()
endmacro()


# -----------------------------------------------------------------------------
# Step 2: Partition the sources into libraries with one-line calls.
# -----------------------------------------------------------------------------

# Partition some of the sources into separate object libraries, removing them from the global list FASTLED_SOURCES.
add_partitioned_library(FastLED_fx          ".*/fx/")
add_partitioned_library(FastLED_platforms   ".*/platforms/")
# Uncomment the next line if you wish to partition lib8tion sources:
# add_partitioned_library(fastled8tion ".*/lib8tion/")
add_partitioned_library(FastLED_fl          ".*/fl/")
add_partitioned_library(FastLED_sensors     ".*/sensors/")
add_partitioned_library(FastLED_third_party ".*/third_party/")
# Anything else will be considered the "core" library

# The remaining files become the "core" library.
if(FASTLED_SOURCES)
    add_library(FastLED_core OBJECT ${FASTLED_SOURCES})
    target_compile_options(FastLED_core PRIVATE ${COMMON_COMPILE_FLAGS})
    target_compile_definitions(FastLED_core PRIVATE ${COMMON_COMPILE_DEFINITIONS})
    list(APPEND FASTLED_OBJECT_LIBS FastLED_core)
else()
    message(FATAL_ERROR "Error: FastLED_core has no sources! This should not happen.")
endif()


# -----------------------------------------------------------------------------
# Create the main FastLED library by aggregating all object libraries.
# -----------------------------------------------------------------------------

add_library(fastled STATIC)

# Loop over the global list of object libraries and add their object files.
foreach(obj_lib IN LISTS FASTLED_OBJECT_LIBS)
    target_sources(fastled PRIVATE $<TARGET_OBJECTS:${obj_lib}>)
endforeach()

# Forward the compile flags and definitions from the parent.
target_compile_options(fastled PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_definitions(fastled PRIVATE ${COMMON_COMPILE_DEFINITIONS})

# Ensure full archive linking: force inclusion of all object files.
target_link_options(fastled PRIVATE "-Wl,--whole-archive" "-Wl,--no-whole-archive")

