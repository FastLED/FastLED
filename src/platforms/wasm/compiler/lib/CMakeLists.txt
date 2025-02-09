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

# if(BUILD_MODE STREQUAL "QUICK")
#     set(OPT_LEVEL "-O1")
# elseif(BUILD_MODE STREQUAL "DEBUG")
#     set(OPT_LEVEL "-Og")
# else() # RELEASE
#     set(OPT_LEVEL "-Oz")
# endif()


set(OPT_LEVEL "-Oz")

# --- Common Compiler and Linker Flags ---
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
    --bind
    -DUSE_OFFSET_CONVERTER=0
    -sINITIAL_MEMORY=134217728
    -fuse-ld=lld
    --no-entry
    -sMODULARIZE=1
    -sUSE_PTHREADS=0          # Disable threading support
)

set(LIBRARY_LINK_FLAGS
    ${COMMON_LINK_FLAGS}

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
endif()

# src\platforms\wasm\compiler\lib\CMakeLists.txt

# --- FASTLED LIBRARY SETUP ---
# Set the FastLED source directory (assumes your FastLED sources are in fastled/src)
set(FASTLED_SOURCE_DIR ../../../../)
# set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fastled/src)
message(STATUS "fastled source directory: ${FASTLED_SOURCE_DIR}")


# Enable header copying mode.
set(FASTLED_COPY_HEADERS ON CACHE BOOL "Copy FastLED headers to the include path")
set(FASTLED_INCLUDE_DEST /include CACHE PATH "Destination path for FastLED headers")

# (Optional) Add the FastLED headers to the global include path.
include_directories(${FASTLED_SOURCE_DIR})

# Delegate building the FastLED library (and header copying) to the subdirectory.
add_subdirectory(${FASTLED_SOURCE_DIR} ${CMAKE_BINARY_DIR}/fastled)
# Add FastLED headers to the global include path.








# Set target properties for fastled (if it exists)
if(TARGET fastled)
    set_target_properties(fastled PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# # --- EXECUTABLE (SKETCH) SETUP ---
# # Instead of using the FastLED sources directly, we build an executable from local sketch sources.
# file(GLOB_RECURSE EXECUTABLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
# message(STATUS "Executable sources: ${EXECUTABLE_SOURCES}")

# add_executable(sketch ${EXECUTABLE_SOURCES})

# # Set compile options and definitions for the sketch executable.
# target_compile_options(sketch PRIVATE ${COMMON_COMPILE_FLAGS})
# target_compile_definitions(sketch PRIVATE SKETCH_COMPILE)
# target_include_directories(sketch PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/fastled/src
#     ${CMAKE_CURRENT_SOURCE_DIR}/src
# )
# target_link_options(sketch PRIVATE ${SKETCH_LINK_FLAGS})
# set_target_properties(sketch PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED ON
# )
# target_link_libraries(sketch PRIVATE fastled)