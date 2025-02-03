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

# Linker flags
set(COMMON_LINK_FLAGS
    -sALLOW_MEMORY_GROWTH=0
    -sDISABLE_EXCEPTION_CATCHING=1
    -sDISABLE_EXCEPTION_THROWING=0
    --bind
    -DUSE_OFFSET_CONVERTER=0
    -sINITIAL_MEMORY=134217728
    -fuse-ld=lld
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']
    -sEXPORTED_FUNCTIONS=['_malloc','_free','_extern_setup','_extern_loop','_fastled_declare_files']
    --no-entry
    -sMODULARIZE=1
    -sEXPORT_NAME=fastled
    -o fastled.js
)

# Debug-specific flags
if(BUILD_MODE STREQUAL "DEBUG")
    list(APPEND COMMON_COMPILE_FLAGS
        -g3
        -fsanitize=address
        -fsanitize=undefined
    )
    list(APPEND COMMON_LINK_FLAGS
        -gsource-map
        --emit-symbol-map
        -sSTACK_OVERFLOW_CHECK=2
        -ASSERTIONS=1
    )
endif()


# --- LIBRARY SECTION ---
# Set FastLED_lib source directory and include it (adds -I fastled/src)
set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fastled/src)
message(STATUS "FastLED_lib source directory: ${FASTLED_SOURCE_DIR}")
include_directories(${FASTLED_SOURCE_DIR})

# Find all FastLED_lib source files
file(GLOB_RECURSE FASTLED_SOURCES 
    "${FASTLED_SOURCE_DIR}/*.cpp"
)
message(STATUS "FastLED_lib sources: ${FASTLED_SOURCES}")

# Create core library with essential components
file(GLOB CORE_SOURCES "${FASTLED_SOURCE_DIR}/*.cpp")
message(STATUS "Core Sources: ${CORE_SOURCES}")
message(STATUS "Current Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")
add_library(FastLED_core OBJECT ${CORE_SOURCES})
set_target_properties(FastLED_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
target_compile_options(FastLED_core PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_definitions(FastLED_core PRIVATE ${COMMON_COMPILE_DEFINITIONS})

# Create component libraries as OBJECT libraries (if any)
if(FX_SOURCES)
    add_library(FastLED_fx OBJECT ${FX_SOURCES})
    set_target_properties(FastLED_fx PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_fx PRIVATE ${COMMON_COMPILE_FLAGS})
    target_compile_definitions(FastLED_fx PRIVATE ${COMMON_COMPILE_DEFINITIONS})
endif()

if(PLATFORMS_SOURCES)
    add_library(FastLED_platforms OBJECT ${PLATFORMS_SOURCES})
    set_target_properties(FastLED_platforms PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_platforms PRIVATE ${COMMON_COMPILE_FLAGS})
    target_compile_definitions(FastLED_platforms PRIVATE ${COMMON_COMPILE_DEFINITIONS})
endif()

if(LIB8TION_SOURCES)
    add_library(FastLED_lib8tion OBJECT ${LIB8TION_SOURCES})
    set_target_properties(FastLED_lib8tion PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_lib8tion PRIVATE ${COMMON_COMPILE_FLAGS})
    target_compile_definitions(FastLED_lib8tion PRIVATE ${COMMON_COMPILE_DEFINITIONS})
endif()

if(FL_SOURCES)
    add_library(FastLED_fl OBJECT ${FL_SOURCES})
    set_target_properties(FastLED_fl PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_fl PRIVATE ${COMMON_COMPILE_FLAGS})
    target_compile_definitions(FastLED_fl PRIVATE ${COMMON_COMPILE_DEFINITIONS})
endif()

if(SENSORS_SOURCES)
    add_library(FastLED_sensors OBJECT ${SENSORS_SOURCES})
    set_target_properties(FastLED_sensors PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(FastLED_sensors PRIVATE ${COMMON_COMPILE_FLAGS})
    target_compile_definitions(FastLED_sensors PRIVATE ${COMMON_COMPILE_DEFINITIONS})
endif()

# Create the main FastLED_lib static library that includes all components
add_library(FastLED_lib STATIC 
    $<TARGET_OBJECTS:FastLED_core>
    $<$<TARGET_EXISTS:FastLED_fx>:$<TARGET_OBJECTS:FastLED_fx>>
    $<$<TARGET_EXISTS:FastLED_platforms>:$<TARGET_OBJECTS:FastLED_platforms>>
    $<$<TARGET_EXISTS:FastLED_lib8tion>:$<TARGET_OBJECTS:FastLED_lib8tion>>
    $<$<TARGET_EXISTS:FastLED_fl>:$<TARGET_OBJECTS:FastLED_fl>>
    $<$<TARGET_EXISTS:FastLED_sensors>:$<TARGET_OBJECTS:FastLED_sensors>>
)
# Note: The FastLED_lib target (the static library) gets its compile definitions
# via the OBJECT libraries and from this target below.
target_compile_definitions(FastLED_lib PRIVATE ${COMMON_COMPILE_DEFINITIONS})
set_target_properties(FastLED_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Add whole archive linking for FastLED_lib
target_link_options(FastLED_lib PRIVATE "-Wl,--whole-archive" $<TARGET_FILE:FastLED_lib> "-Wl,--no-whole-archive")

# Remove platform-specific files that might cause issues
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")

# --- EXECUTABLE SECTION ---
# Instead of using the FastLED_lib sources for the executable, now use sources from the local src/ directory.
file(GLOB_RECURSE EXECUTABLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
message(STATUS "Executable sources: ${EXECUTABLE_SOURCES}")

# Create the WASM executable using the new EXECUTABLE_SOURCES
add_executable(fastled_sketch ${EXECUTABLE_SOURCES})
# Set properties and flags for the executable
target_compile_options(fastled_sketch PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_definitions(fastled_sketch PRIVATE SKETCH_COMPILE)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/fastled/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src  # Adjust the path accordingly
)
target_link_options(fastled_sketch PRIVATE ${COMMON_LINK_FLAGS})
set_target_properties(fastled_sketch PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# (Optional) Here you can add additional compile definitions for the executable target,
# which will be applied after the library definitions.
# For example:
# target_compile_definitions(fastled_sketch PRIVATE EXECUTABLE_SPECIFIC_DEFINE=1)

# Add verbose output for tests
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
