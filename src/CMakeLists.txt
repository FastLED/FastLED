# Set FastLED source directory
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "FASTLED_SOURCE_DIR: ${FASTLED_SOURCE_DIR}")

# Verify the directory exists
if(NOT EXISTS ${FASTLED_SOURCE_DIR})
    message(FATAL_ERROR "Error: FASTLED_SOURCE_DIR does not exist! Check directory path.")
endif()

# Include FastLED headers
include_directories(${FASTLED_SOURCE_DIR})

# Recursively find all FastLED source files
file(GLOB_RECURSE FASTLED_ALL_SOURCES "${FASTLED_SOURCE_DIR}/*.cpp")

# Debugging output: Check what files were found
message(STATUS "Found source files: ${FASTLED_ALL_SOURCES}")

# If no files were found, print an error message
if("${FASTLED_ALL_SOURCES}" STREQUAL "")
    message(FATAL_ERROR "Error: No source files found in ${FASTLED_SOURCE_DIR}!")
endif()

# Exclude platform-specific files
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*avr.*")

# Debugging output after filtering
message(STATUS "Filtered source files: ${FASTLED_ALL_SOURCES}")

# Categorize source files
set(FX_SOURCES "")
set(PLATFORMS_SOURCES "")
set(LIB8TION_SOURCES "")
set(FL_SOURCES "")
set(SENSORS_SOURCES "")
set(OTHER_SOURCES "")

foreach(SOURCE ${FASTLED_ALL_SOURCES})
    message(STATUS "Processing source file: ${SOURCE}")

    if(SOURCE MATCHES "/fx/")
        list(APPEND FX_SOURCES "${SOURCE}")
    elseif(SOURCE MATCHES "/platforms/")
        list(APPEND PLATFORMS_SOURCES "${SOURCE}")
    elseif(SOURCE MATCHES "/lib8tion/")
        list(APPEND LIB8TION_SOURCES "${SOURCE}")
    elseif(SOURCE MATCHES "/fl/")
        list(APPEND FL_SOURCES "${SOURCE}")
    elseif(SOURCE MATCHES "/sensors/")
        list(APPEND SENSORS_SOURCES "${SOURCE}")
    else()
        list(APPEND OTHER_SOURCES "${SOURCE}")
    endif()
endforeach()

# Debugging: Print out categorized files
message(STATUS "OTHER_SOURCES: ${OTHER_SOURCES}")
message(STATUS "FX_SOURCES: ${FX_SOURCES}")
message(STATUS "PLATFORMS_SOURCES: ${PLATFORMS_SOURCES}")
message(STATUS "LIB8TION_SOURCES: ${LIB8TION_SOURCES}")
message(STATUS "FL_SOURCES: ${FL_SOURCES}")
message(STATUS "SENSORS_SOURCES: ${SENSORS_SOURCES}")

# Ensure lists are not empty
if("${OTHER_SOURCES}" STREQUAL "")
    message(FATAL_ERROR "Error: FastLED_core has no sources! This should not happen.")
endif()

# Define a macro for adding FastLED libraries
macro(add_fastled_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    if(NOT ARG_SOURCES OR ARG_SOURCES STREQUAL "")
        message(FATAL_ERROR "Error: ${name} has no sources! This should not happen. List received: '${ARG_SOURCES}'")
    endif()
    
    add_library(${name} OBJECT ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(${name} PRIVATE ${COMMON_COMPILE_FLAGS})
endmacro()

# Create object libraries for each source group (fix: properly pass lists)
add_fastled_library(FastLED_core SOURCES ${OTHER_SOURCES})
add_fastled_library(FastLED_fx SOURCES ${FX_SOURCES})
add_fastled_library(FastLED_platforms SOURCES ${PLATFORMS_SOURCES})
add_fastled_library(FastLED_lib8tion SOURCES ${LIB8TION_SOURCES})
add_fastled_library(FastLED_fl SOURCES ${FL_SOURCES})
add_fastled_library(FastLED_sensors SOURCES ${SENSORS_SOURCES})

# Create main FastLED library (fix: ensure target objects reference properly)
add_library(FastLED_lib STATIC
    $<TARGET_OBJECTS:FastLED_core>
    $<$<BOOL:${FX_SOURCES}>:$<TARGET_OBJECTS:FastLED_fx>>
    $<$<BOOL:${PLATFORMS_SOURCES}>:$<TARGET_OBJECTS:FastLED_platforms>>
    $<$<BOOL:${LIB8TION_SOURCES}>:$<TARGET_OBJECTS:FastLED_lib8tion>>
    $<$<BOOL:${FL_SOURCES}>:$<TARGET_OBJECTS:FastLED_fl>>
    $<$<BOOL:${SENSORS_SOURCES}>:$<TARGET_OBJECTS:FastLED_sensors>>
)

set_target_properties(FastLED_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

target_link_options(FastLED_lib PRIVATE "-Wl,--whole-archive" "-Wl,--no-whole-archive")
