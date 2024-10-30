import os

DEBUG = 0
USE_CCACHE = True

if "DEBUG" in os.environ:
    DEBUG = 1

if "NO_CCACHE" in os.environ:
    USE_CCACHE = False

# Global variable to control WASM output (0 for asm.js, 1 for WebAssembly)
# It seems easier to load the program as a pure JS file, so we will use asm.js
# right now as a test.
USE_WASM = 2

if DEBUG:
    USE_WASM=1

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

wasmflags = [
    "-DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50",
    "-DFASTLED_FORCE_NAMESPACE=1",
    "-DFASTLED_USE_PROGMEM=0",
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']",
    "-sALLOW_MEMORY_GROWTH=0",
    "-Oz",
    #"-sEXPORT_ES6=1",
    "-sEXPORTED_FUNCTIONS=['_malloc','_free','_extern_setup','_extern_loop','_fastled_declare_files']",
    "--bind",
    "-DUSE_OFFSET_CONVERTER=0",
    "-sINITIAL_MEMORY=134217728",
    "--no-entry",
    "-s",
    # Enable C++17 with GNU extensions.
    "-std=gnu++17",
    "-fpermissive",
    "-Wno-constant-logical-operand",
    "-Wnon-c-typedef-for-linkage",
    f"-sWASM={USE_WASM}",
]

if DEBUG:
    wasmflags += [
        '-g3',
        '-gsource-map',
        '--emit-symbol-map',
        #'-sSTACK_OVERFLOW_CHECK=2',
        #'-sSAFE_HEAP=1',
        #'-sSAFE_HEAP_LOG=1',
        '-ASSERTIONS=1',
        # sanitize address
        '-fsanitize=address',
        '-fsanitize=undefined',
    ]
    # Remove -Oz flag
    opt_flags = ["-Oz", "-Os", "-O1", "-O2", "-O3"]
    for opt in opt_flags:
        if opt in wasmflags:
            wasmflags.remove(opt)
    


export_name = env.GetProjectOption("custom_wasm_export_name", "")
if export_name:
    wasmflags += [
        "-s",
        "MODULARIZE=1",
        "-s",
        f"EXPORT_NAME='{export_name}'",
        "-o",
        f"{env.subst('$BUILD_DIR')}/{export_name}.js",
    ]

env.Append(LINKFLAGS=wasmflags)


# Pass flags to the other Project Dependencies (libraries)
for lb in env.GetLibBuilders():
    lb.env.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")
    # Add whole-archive flag to ensure all objects have all symbols available
    # for final linking.
    lb.env.Append(LINKFLAGS=["-Wl,--whole-archive"])


