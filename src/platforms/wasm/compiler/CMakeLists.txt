cmake_minimum_required(VERSION 3.10)
project(FastLED_WASM)

# Enable verbose makefile output for all compiled sources
set(CMAKE_VERBOSE_MAKEFILE ON)

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

# (Optional: PROFILE flags can be added here if needed)

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
set(LIBFASTLED_PATH /precompiled/libfastled.a)

if(EXISTS ${LIBFASTLED_PATH})
    message(STATUS "Found prebuilt libfastled.a at ${LIBFASTLED_PATH}, skipping build of FastLED library.")
    add_library(fastled STATIC IMPORTED)
    set_target_properties(fastled PROPERTIES IMPORTED_LOCATION ${LIBFASTLED_PATH})
else()
    message(FATAL_ERROR "Did not find precompiled libfastled.a at ${LIBFASTLED_PATH}. Please build the library first.")
endif()

if(TARGET fastled)
    set_target_properties(fastled PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# --- LIBRARY TARGET: sketch_lib ---
file(GLOB_RECURSE ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
message(STATUS "All sources: ${ALL_SOURCES}")

add_library(sketch_lib STATIC ${ALL_SOURCES})

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

# ── FINAL LINKING STEP: Produce fastled.js ──
# This custom target invokes em++ to link the static library (sketch_lib)
# with the precompiled libfastled.a into the final binary fastled.js.
add_custom_target(final_wasm ALL
    COMMAND em++ ${SKETCH_LINK_FLAGS} -Wl,--whole-archive $<TARGET_FILE:sketch_lib> -Wl,--no-whole-archive /precompiled/libfastled.a
    DEPENDS sketch_lib fastled
    COMMENT "Linking final wasm binary to fastled.js"
)
