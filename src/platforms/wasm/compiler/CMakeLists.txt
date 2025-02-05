cmake_minimum_required(VERSION 3.10)
project(FastLED_WASM)

# Enable verbose makefile output for all compiled sources
set(CMAKE_VERBOSE_MAKEFILE ON)


# projenv.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")
# env.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")

# --- Set Emscripten as the Compiler ---
set(CMAKE_C_COMPILER "emcc")
set(CMAKE_CXX_COMPILER "em++")
set(CMAKE_AR "emar")
set(CMAKE_RANLIB "emranlib")

# Use ccache as a compiler launcher (requires CMake 3.4 or newer)
set(CMAKE_C_COMPILER_LAUNCHER "ccache")
set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")


# --- Build Mode and Optimization Level ---
if(NOT DEFINED ENV{BUILD_MODE})
    set(BUILD_MODE "QUICK")
else()
    set(BUILD_MODE $ENV{BUILD_MODE})
endif()

if(BUILD_MODE STREQUAL "QUICK")
    set(OPT_LEVEL "-O0")
elseif(BUILD_MODE STREQUAL "DEBUG")
    set(OPT_LEVEL "-Og")
else() # RELEASE
    set(OPT_LEVEL "-Oz")
endif()

# --- Common Compiler and Linker Flags ---
set(COMMON_COMPILE_FLAGS
    -DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50
    -DFASTLED_FORCE_NAMESPACE=1
    -DFASTLED_NO_FORCE_INLINE
    -DFASTLED_USE_PROGMEM=0
    -DDISABLE_EXCEPTION_CATCHING=1
    -fno-exceptions
    -fno-rtti
    -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0
    -std=gnu++17
    -fpermissive
    -fno-inline
    -Wno-constant-logical-operand
    -Wnon-c-typedef-for-linkage
    -Werror=bad-function-cast
    -Werror=cast-function-type
    -sUSE_PTHREADS=0          # Disable threading support
    ${OPT_LEVEL}
)




# If PROFILE flag is passed (e.g. via -DPROFILE), add Clang statistics flags.
# if(DEFINED PROFILE)
#     message(STATUS "PROFILE flag passed: enabling Clang compiler stats output")
#     list(APPEND COMMON_COMPILE_FLAGS -Xclang -print-stats -mllvm -stats -DLLVM_FORCE_ENABLE_STATS=1)
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLLVM_FORCE_ENABLE_STATS")
# else()
#     message(STATUS "PROFILE flag not passed: skipping Clang compiler stats output")
# endif()

set(COMMON_LINK_FLAGS
    -sALLOW_MEMORY_GROWTH=0
    -sDISABLE_EXCEPTION_CATCHING=1
    -sDISABLE_EXCEPTION_THROWING=0
    -sWASM_BIGINT
    --bind
    -DUSE_OFFSET_CONVERTER=0
    -sINITIAL_MEMORY=134217728
    -fuse-ld=lld
    --no-entry
    -sMODULARIZE=1
)

set(LIBRARY_LINK_FLAGS
    ${COMMON_LINK_FLAGS}

)

set(SKETCH_LINK_FLAGS
    ${COMMON_LINK_FLAGS}
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']
    -sEXPORTED_FUNCTIONS=['_malloc','_free','_extern_setup','_extern_loop','_fastled_declare_files']
    -sEXPORT_NAME=fastled
    -o fastled.js
)

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
# Set the FastLED source directory (assumes your FastLED sources are in fastled/src)
#set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fastled/src)
#message(STATUS "fastled source directory: ${FASTLED_SOURCE_DIR}")


# Add FastLED headers to the global include path.


# Check for a prebuilt libfastled.a. If it exists, create an imported target;
# otherwise, build FastLED from source.
set(LIBFASTLED_PATH /precompiled/libfastled.a)

if(EXISTS ${LIBFASTLED_PATH})
    message(STATUS "Found prebuilt libfastled.a at ${LIBFASTLED_PATH}, skipping build of FastLED library.")
    add_library(fastled STATIC IMPORTED)
    set_target_properties(fastled PROPERTIES IMPORTED_LOCATION ${LIBFASTLED_PATH})

else()
    message(FATAL_ERROR "Did not find prebuilt libfastled.a at ${LIBFASTLED_PATH}. Please build the library first.")
endif()

# Set target properties for fastled (if it exists)
if(TARGET fastled)
    set_target_properties(fastled PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# --- EXECUTABLE (SKETCH) SETUP ---
# Gather all cpp source files
file(GLOB_RECURSE ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
message(STATUS "All sources: ${ALL_SOURCES}")

# Create the sketch library and executable
add_library(sketch_lib STATIC ${ALL_SOURCES})
add_executable(sketch EXCLUDE_FROM_ALL ${ALL_SOURCES})

# Set compile options for the sketch library
target_compile_options(sketch_lib PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_definitions(sketch_lib PRIVATE SKETCH_COMPILE)
target_include_directories(sketch_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    "/precompiled/include"
)
set_target_properties(sketch_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Set properties for the final executable
target_compile_options(sketch PRIVATE ${COMMON_COMPILE_FLAGS})
target_link_options(sketch PRIVATE ${SKETCH_LINK_FLAGS})
set_target_properties(sketch PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Link the executable with both libraries
target_link_libraries(sketch PRIVATE sketch_lib fastled)
