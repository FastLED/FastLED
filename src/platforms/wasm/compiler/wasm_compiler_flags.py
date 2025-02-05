import os

USE_CCACHE = True if "NO_CCACHE" not in os.environ else False

# Get build mode from environment variable, default to RELEASE if not set
BUILD_MODE = os.environ.get("BUILD_MODE", "QUICK").upper()

# Validate build mode
valid_modes = ["DEBUG", "QUICK", "RELEASE"]
if BUILD_MODE not in valid_modes:
    raise ValueError(f"BUILD_MODE must be one of {valid_modes}, got {BUILD_MODE}")

# Set flags based on build mode
DEBUG = BUILD_MODE == "DEBUG"
QUICK_BUILD = BUILD_MODE == "QUICK" 
OPTIMIZED = BUILD_MODE == "RELEASE"

# Global variable to control WASM output (0 for asm.js, 1 for WebAssembly)
# It seems easier to load the program as a pure JS file, so we will use asm.js
# right now as a test.
USE_WASM = 2

if DEBUG or QUICK_BUILD:
    USE_WASM=1  # disable wasm2js on these builds.

build_mode = "-O1" if QUICK_BUILD else "-Oz"

Import("env", "projenv")

# projenv is used for compiling individual files, env for linking
# libraries have their own env in env.GetLibBuilders()

# this is kinda hacky but let's just replace the default toolchain
# with emscripten. the right way to do this would be to create a
# SCons toolchain file for emscripten, and platformio platform for
# WebAssembly, but this is easier for now

CC = "ccache emcc" if USE_CCACHE else "emcc"
CXX = "ccache em++" if USE_CCACHE else "em++"
LINK = "ccache em++" if USE_CCACHE else "em++"

projenv.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")
env.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")

# Todo: Investigate the following flags
# -sSINGLE_FILE=1

sketch_flags = [
    "-DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50",
    "-DFASTLED_FORCE_NAMESPACE=1",
    "-DFASTLED_USE_PROGMEM=0",
    #"-DDISABLE_EXCEPTION_CATCHING=1",
    "-sALLOW_MEMORY_GROWTH=0",
    #"-fno-exceptions",
    #"-fno-rtti",
    #"-DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0",
    #"-sDISABLE_EXCEPTION_CATCHING=1",
    #"-sDISABLE_EXCEPTION_THROWING=0",
    build_mode,
    "--bind",
    "-DUSE_OFFSET_CONVERTER=0",
    "-sINITIAL_MEMORY=134217728",
    "-std=gnu++17",
    "-fpermissive",
    "-Wno-constant-logical-operand",
    "-Wnon-c-typedef-for-linkage",
    f"-sWASM={USE_WASM}",
    "-fuse-ld=lld",
    #-Wbad-function-cast -Wcast-function
    "-Werror=bad-function-cast",
    "-Werror=cast-function-type",
    # add /js/src/ to the include path
    "-I",
    "src",
]

if QUICK_BUILD:
    sketch_flags += ["-sERROR_ON_WASM_CHANGES_AFTER_LINK", "-sWASM_BIGINT"]
elif DEBUG:
    # Remove -Oz flag
    opt_flags = ["-Oz", "-Os", "-O0", "-O1", "-O2", "-O3"]
    for opt in opt_flags:
        if opt in sketch_flags:
            sketch_flags.remove(opt)
    sketch_flags += [
        '-g3',
        '-gsource-map',
        '--emit-symbol-map',
        '-sSTACK_OVERFLOW_CHECK=2',
        '-ASSERTIONS=1',
        '-fsanitize=address',
        '-fsanitize=undefined',
    ]


sketch_flags += [
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']",
    "-sEXPORTED_FUNCTIONS=['_malloc','_free','_extern_setup','_extern_loop','_fastled_declare_files']",
    "--no-entry",
    #"-fno-exceptions",
    #"-fno-rtti",
    #"-DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0",
    #"-sDISABLE_EXCEPTION_CATCHING=1",
    #"-sDISABLE_EXCEPTION_THROWING=0",
]

if OPTIMIZED:
    sketch_flags += ["-flto"]

export_name = env.GetProjectOption("custom_wasm_export_name", "")
if export_name:
    sketch_flags += [
        "-s",
        "MODULARIZE=1",
        "-s",
        f"EXPORT_NAME='{export_name}'",
        "-o",
        f"{env.subst('$BUILD_DIR')}/{export_name}.js",
    ]

env.Append(LINKFLAGS=sketch_flags)


fastled_compile_cc_flags = [
    "-Werror=bad-function-cast",
    "-Werror=cast-function-type",
    #"-fno-exceptions",
    #"-fno-rtti",
    build_mode,
    #"-DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0",
    #"-fno-exceptions",
    #"-sDISABLE_EXCEPTION_CATCHING=1",
    #"-sDISABLE_EXCEPTION_THROWING=0",
]




fastled_compile_link_flags = [
    "-Wl,--whole-archive,-fuse-ld=lld",
    "-Werror=bad-function-cast",
    "-Werror=cast-function-type",
]


# Pass flags to the other Project Dependencies (libraries)
for lb in env.GetLibBuilders():
    lb.env.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")
    # Add whole-archive flag to ensure all objects have all symbols available
    # for final linking.
    lb.env.Append(CCFLAGS=fastled_compile_cc_flags)
    lb.env.Append(LINKFLAGS=fastled_compile_link_flags)


