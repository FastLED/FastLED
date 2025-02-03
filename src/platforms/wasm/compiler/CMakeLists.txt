cmake_minimum_required(VERSION 3.10)
project(FastLED_WASM)

# Enable verbose makefile output for all compiled sources
set(CMAKE_VERBOSE_MAKEFILE ON)

# Set Emscripten as the compiler
set(CMAKE_C_COMPILER emcc)
set(CMAKE_CXX_COMPILER em++)

# Get build mode from environment, default to QUICK
if(NOT DEFINED ENV{BUILD_MODE})
    set(BUILD_MODE "QUICK")
else()
    set(BUILD_MODE $ENV{BUILD_MODE})
endif()

# Set optimization level based on build mode
if(BUILD_MODE STREQUAL "QUICK")
    set(OPT_LEVEL "-O1")
elseif(BUILD_MODE STREQUAL "DEBUG")
    set(OPT_LEVEL "-O0")
else() # RELEASE
    set(OPT_LEVEL "-Oz")
endif()

# Common compiler flags
set(COMMON_COMPILE_FLAGS
    -DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50
    -DFASTLED_FORCE_NAMESPACE=1
    -DFASTLED_USE_PROGMEM=0
    -DDISABLE_EXCEPTION_CATCHING=1
    -fno-exceptions
    -fno-rtti
    -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0
    -std=gnu++17
    -fpermissive
    -Wno-constant-logical-operand
    -Wnon-c-typedef-for-linkage
    -Werror=bad-function-cast
    -Werror=cast-function-type
    -v
    ${OPT_LEVEL}
)

# Common linker flags for both the library and the executable.
set(COMMON_LINK_FLAGS
    -sALLOW_MEMORY_GROWTH=0
    -sDISABLE_EXCEPTION_CATCHING=1
    -sDISABLE_EXCEPTION_THROWING=0
    --bind
    -DUSE_OFFSET_CONVERTER=0
    -sINITIAL_MEMORY=134217728
    -fuse-ld=lld
    --no-entry
    -sMODULARIZE=1
    -sEXPORT_NAME=fastled
)

# Additional linker flags specific to the library.
set(LIBRARY_LINK_FLAGS
    ${COMMON_LINK_FLAGS}
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']
    -sEXPORTED_FUNCTIONS=['_malloc','_free','_extern_setup','_extern_loop','_fastled_declare_files']
)

# Additional linker flags specific to the sketch executable.
set(SKETCH_LINK_FLAGS
    ${COMMON_LINK_FLAGS}
    #-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']
    #-sEXPORTED_FUNCTIONS=['_malloc','_free','_extern_setup','_extern_loop','_fastled_declare_files']
    -o fastled.js
)

# Debug-specific flags
if(BUILD_MODE STREQUAL "DEBUG")
    list(APPEND COMMON_COMPILE_FLAGS
        -g3
        -fsanitize=address
        -fsanitize=undefined
    )
    set(DEBUG_LINK_FLAGS
        -gsource-map
        --emit-symbol-map
        -sSTACK_OVERFLOW_CHECK=2
        -ASSERTIONS=1
    )
    list(APPEND LIBRARY_LINK_FLAGS ${DEBUG_LINK_FLAGS})
    list(APPEND SKETCH_LINK_FLAGS ${DEBUG_LINK_FLAGS})
endif()

# --- FASTLED LIBRARY SETUP ---

# Set FastLED_lib source directory (assumes your FastLED sources are in fastled/src relative to your top-level directory)
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fastled/src)
message(STATUS "FastLED_lib source directory: ${FASTLED_SOURCE_DIR}")

# Include FastLED headers
include_directories(${FASTLED_SOURCE_DIR})

# Recursively find all FastLED source files
file(GLOB_RECURSE FASTLED_ALL_SOURCES "${FASTLED_SOURCE_DIR}/*.cpp")

# (Optional) Remove platform-specific files that might cause issues
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_ALL_SOURCES EXCLUDE REGEX ".*avr.*")

# Group the sources by subdirectory. Files not in one of these subdirectories will go into OTHER_SOURCES.
set(FX_SOURCES "")
set(PLATFORMS_SOURCES "")
set(LIB8TION_SOURCES "")
set(FL_SOURCES "")
set(SENSORS_SOURCES "")
set(OTHER_SOURCES "")

foreach(SOURCE ${FASTLED_ALL_SOURCES})
    if(SOURCE MATCHES "/fx/")
        list(APPEND FX_SOURCES ${SOURCE})
    elseif(SOURCE MATCHES "/platforms/")
        list(APPEND PLATFORMS_SOURCES ${SOURCE})
    elseif(SOURCE MATCHES "/lib8tion/")
        list(APPEND LIB8TION_SOURCES ${SOURCE})
    elseif(SOURCE MATCHES "/fl/")
        list(APPEND FL_SOURCES ${SOURCE})
    elseif(SOURCE MATCHES "/sensors/")
        list(APPEND SENSORS_SOURCES ${SOURCE})
    else()
        list(APPEND OTHER_SOURCES ${SOURCE})
    endif()
endforeach()

message(STATUS "Other Sources: ${OTHER_SOURCES}")
message(STATUS "FX Sources: ${FX_SOURCES}")
message(STATUS "Platforms Sources: ${PLATFORMS_SOURCES}")
message(STATUS "Lib8tion Sources: ${LIB8TION_SOURCES}")
message(STATUS "FL Sources: ${FL_SOURCES}")
message(STATUS "Sensors Sources: ${SENSORS_SOURCES}")

# Create OBJECT libraries for each group

# Core (files not in a subdirectory group)
if(OTHER_SOURCES)
    add_library(FastLED_core OBJECT ${OTHER_SOURCES})
    set_target_properties(FastLED_core PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_core PRIVATE ${COMMON_COMPILE_FLAGS})
endif()

if(FX_SOURCES)
    add_library(FastLED_fx OBJECT ${FX_SOURCES})
    set_target_properties(FastLED_fx PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_fx PRIVATE ${COMMON_COMPILE_FLAGS})
endif()

if(PLATFORMS_SOURCES)
    add_library(FastLED_platforms OBJECT ${PLATFORMS_SOURCES})
    set_target_properties(FastLED_platforms PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_platforms PRIVATE ${COMMON_COMPILE_FLAGS})
endif()

if(LIB8TION_SOURCES)
    add_library(FastLED_lib8tion OBJECT ${LIB8TION_SOURCES})
    set_target_properties(FastLED_lib8tion PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_lib8tion PRIVATE ${COMMON_COMPILE_FLAGS})
endif()

if(FL_SOURCES)
    add_library(FastLED_fl OBJECT ${FL_SOURCES})
    set_target_properties(FastLED_fl PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_fl PRIVATE ${COMMON_COMPILE_FLAGS})
endif()

if(SENSORS_SOURCES)
    add_library(FastLED_sensors OBJECT ${SENSORS_SOURCES})
    set_target_properties(FastLED_sensors PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_sensors PRIVATE ${COMMON_COMPILE_FLAGS})
endif()

# Create the main FastLED_lib static library that includes all components
add_library(FastLED_lib STATIC 
    $<$<BOOL:${OTHER_SOURCES}>:$<TARGET_OBJECTS:FastLED_core>>
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

target_link_options(FastLED_lib PRIVATE
    "-Wl,--whole-archive" "-Wl,--no-whole-archive"
)

# (Optional) Pass any compile definitions to the library here:
# target_compile_definitions(FastLED_lib PRIVATE ...)

# (Optional) If you need to force whole-archive linking, consider applying the flags at the executable stage.
# For example:
# target_link_options(fastled_sketch PRIVATE "-Wl,--whole-archive" $<TARGET_FILE:FastLED_lib> "-Wl,--no-whole-archive")

# --- EXECUTABLE SECTION ---
# Instead of using the FastLED_lib sources for the executable, now use sources from the local src/ directory.
file(GLOB_RECURSE EXECUTABLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
message(STATUS "Executable sources: ${EXECUTABLE_SOURCES}")

# Create the WASM executable using the new EXECUTABLE_SOURCES
add_executable(fastled_sketch ${EXECUTABLE_SOURCES})
# Set properties and flags for the executable
target_compile_options(fastled_sketch PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_definitions(fastled_sketch PRIVATE SKETCH_COMPILE)
target_include_directories(fastled_sketch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fastled/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src  # Adjust if needed
)
target_link_options(fastled_sketch PRIVATE ${SKETCH_LINK_FLAGS})
set_target_properties(fastled_sketch PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_link_libraries(fastled_sketch PRIVATE FastLED_lib)
