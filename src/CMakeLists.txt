# Set FastLED source directory (this is where the FastLED sources live)
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "FASTLED_SOURCE_DIR: ${FASTLED_SOURCE_DIR}")

# Retrieve and print the flags passed from the parent (tests/CMakeLists.txt)
message(STATUS "Using compile flags from parent CMakeLists.txt")
message(STATUS "COMMON_COMPILE_FLAGS: ${COMMON_COMPILE_FLAGS}")
message(STATUS "COMMON_COMPILE_DEFINITIONS: ${COMMON_COMPILE_DEFINITIONS}")
# message(FATAL_ERROR "Exiting immediately for debugging purposes")

# Verify the directory exists
if(NOT EXISTS ${FASTLED_SOURCE_DIR})
    message(FATAL_ERROR "Error: FASTLED_SOURCE_DIR does not exist! Check directory path.")
endif()

# Include FastLED headers (assumed to be in this directory)
include_directories(${FASTLED_SOURCE_DIR})

# Recursively find all FastLED source files
file(GLOB_RECURSE FASTLED_ALL_SOURCES "${FASTLED_SOURCE_DIR}/*.cpp")

# Debugging output: list all found source files
message(STATUS "Found source files: ${FASTLED_ALL_SOURCES}")

# If no files were found, print an error message
if("${FASTLED_ALL_SOURCES}" STREQUAL "")
    message(FATAL_ERROR "Error: No source files found in ${FASTLED_SOURCE_DIR}!")
endif()

# Exclude platform-specific files (e.g. ESP, ARM, AVR)
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*avr.*")

# Debugging output after filtering
# message(STATUS "Filtered source files: ${FASTLED_ALL_SOURCES}")

# Categorize source files into groups based on sub-directory patterns
set(FX_SOURCES "")
set(PLATFORMS_SOURCES "")
#set(LIB8TION_SOURCES "")
set(FL_SOURCES "")
set(SENSORS_SOURCES "")
set(OTHER_SOURCES "")

foreach(SOURCE ${FASTLED_ALL_SOURCES})
    message(STATUS "Processing source file: ${SOURCE}")
    if(SOURCE MATCHES "/fx/")
        list(APPEND FX_SOURCES "${SOURCE}")
    elseif(SOURCE MATCHES "/platforms/")
        list(APPEND PLATFORMS_SOURCES "${SOURCE}")
    #elseif(SOURCE MATCHES "/lib8tion/")
    #    list(APPEND LIB8TION_SOURCES "${SOURCE}")
    elseif(SOURCE MATCHES "/fl/")
        list(APPEND FL_SOURCES "${SOURCE}")
    elseif(SOURCE MATCHES "/sensors/")
        list(APPEND SENSORS_SOURCES "${SOURCE}")
    else()
        list(APPEND OTHER_SOURCES "${SOURCE}")
    endif()
endforeach()

# Debug output: display categorized file lists
# message(STATUS "OTHER_SOURCES: ${OTHER_SOURCES}")
# message(STATUS "FX_SOURCES: ${FX_SOURCES}")
# message(STATUS "PLATFORMS_SOURCES: ${PLATFORMS_SOURCES}")
# message(STATUS "LIB8TION_SOURCES: ${LIB8TION_SOURCES}")
# message(STATUS "FL_SOURCES: ${FL_SOURCES}")
# message(STATUS "SENSORS_SOURCES: ${SENSORS_SOURCES}")

if("${OTHER_SOURCES}" STREQUAL "")
    message(FATAL_ERROR "Error: FastLED_core has no sources! This should not happen.")
endif()

# Define a macro to create object libraries for each group.
# Note: Do not set compiler-specific target properties here; let the parent do that.
macro(add_fastled_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    if(NOT ARG_SOURCES OR ARG_SOURCES STREQUAL "")
        message(FATAL_ERROR "Error: ${name} has no sources! List received: '${ARG_SOURCES}'")
    endif()
    
    add_library(${name} OBJECT ${ARG_SOURCES})

    # Apply the compile flags from the parent    # Propagate the parent's compile flags to each object library.
    target_compile_options(${name} PRIVATE ${COMMON_COMPILE_FLAGS})
    
    # Propagate the parent's compile definitions to each object library.
    target_compile_definitions(${name} PRIVATE ${COMMON_COMPILE_DEFINITIONS})
endmacro()

# Create object libraries for each source group
add_fastled_library(FastLED_core SOURCES ${OTHER_SOURCES})
add_fastled_library(FastLED_fx SOURCES ${FX_SOURCES})
add_fastled_library(FastLED_platforms SOURCES ${PLATFORMS_SOURCES})
#add_fastled_library(FastLED_lib8tion SOURCES ${LIB8TION_SOURCES})
add_fastled_library(FastLED_fl SOURCES ${FL_SOURCES})
add_fastled_library(FastLED_sensors SOURCES ${SENSORS_SOURCES})

# Create the main FastLED library (static) that collects all object libraries.
add_library(FastLED_lib STATIC
    $<TARGET_OBJECTS:FastLED_core>
    $<$<BOOL:${FX_SOURCES}>:$<TARGET_OBJECTS:FastLED_fx>>
    $<$<BOOL:${PLATFORMS_SOURCES}>:$<TARGET_OBJECTS:FastLED_platforms>>
    #$<$<BOOL:${LIB8TION_SOURCES}>:$<TARGET_OBJECTS:FastLED_lib8tion>>
    $<$<BOOL:${FL_SOURCES}>:$<TARGET_OBJECTS:FastLED_fl>>
    $<$<BOOL:${SENSORS_SOURCES}>:$<TARGET_OBJECTS:FastLED_sensors>>
)

# Forward the compile flags and definitions from the parent to the final library.
target_compile_options(FastLED_lib PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_definitions(FastLED_lib PRIVATE ${COMMON_COMPILE_DEFINITIONS})

# Ensure full archive linking: force inclusion of all object files.
target_link_options(FastLED_lib PRIVATE "-Wl,--whole-archive" "-Wl,--no-whole-archive")
