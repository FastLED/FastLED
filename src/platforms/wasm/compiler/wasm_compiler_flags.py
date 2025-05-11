# pylint: skip-file
# flake8: noqa
# type: ignore

import os
from SCons.Script import Import

# Determine whether to use ccache
USE_CCACHE = "NO_CCACHE" not in os.environ

# Get build mode from environment variable, default to QUICK if not set
BUILD_MODE = os.environ.get("BUILD_MODE", "QUICK").upper()
valid_modes = ["DEBUG", "QUICK", "RELEASE"]
if BUILD_MODE not in valid_modes:
    raise ValueError(f"BUILD_MODE must be one of {valid_modes}, got {BUILD_MODE}")

DEBUG = BUILD_MODE == "DEBUG"
QUICK_BUILD = BUILD_MODE == "QUICK"
OPTIMIZED = BUILD_MODE == "RELEASE"

# Choose WebAssembly (1), asm.js fallback (2)
USE_WASM = 2
if DEBUG or QUICK_BUILD:
    USE_WASM = 1

# Optimization level
build_mode = "-O1" if QUICK_BUILD else "-Oz"

# Import environments
Import("env", "projenv")

# Build directory
BUILD_DIR = env.subst('$BUILD_DIR')

# Toolchain overrides to use Emscripten
CC = "ccache emcc" if USE_CCACHE else "emcc"
CXX = "ccache em++" if USE_CCACHE else "em++"
LINK = CXX
projenv.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")
env.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")

# Helper to strip out optimization flags
def _remove_flags(curr_flags: list[str], remove_flags: list[str]) -> list[str]:
    for flag in remove_flags:
        if flag in curr_flags:
            curr_flags.remove(flag)
    return curr_flags

# Paths for DWARF split
wasm_name = "fastled.wasm"
wasm_path = f"{BUILD_DIR}/{wasm_name}"

# Base compile flags (CCFLAGS/CXXFLAGS)
compile_flags = [
    "-DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50",
    "-DFASTLED_FORCE_NAMESPACE=1",
    "-DFASTLED_USE_PROGMEM=0",
    build_mode,
    "-DUSE_OFFSET_CONVERTER=0",
    "-std=gnu++17",
    "-fpermissive",
    "-Wno-constant-logical-operand",
    "-Wnon-c-typedef-for-linkage",
    "-Werror=bad-function-cast",
    "-Werror=cast-function-type",
    "-I", "src",
    "-I/js/fastled/src/platforms/wasm/compiler",
]

# Base link flags (LINKFLAGS)
link_flags = [
    "--bind",                                      # ensure embind runtime support is linked in
    "-fuse-ld=lld",                                # use LLD at link time
    f"-sWASM={USE_WASM}",                          # Wasm vs asm.js
    "-sALLOW_MEMORY_GROWTH=1",                     # enable dynamic heap growth
    "-sINITIAL_MEMORY=134217728",                  # start with 128 MB heap
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']",
    "-sEXPORTED_FUNCTIONS=['_malloc','_free','_extern_setup','_extern_loop','_fastled_declare_files']",
    "--no-entry",
]

# Debug-specific flags
debug_compile_flags = [
    "-g3",
    "-gsource-map",
    "-fsanitize=address",
    "-fsanitize=undefined",
    "-fno-inline",
]

debug_link_flags = [
    "--emit-symbol-map",
    # tell LLVM to write out the .dwarf file alongside fastled.wasm
    f"-gseparate-dwarf={BUILD_DIR}/{wasm_name}.dwarf",
    # tell the JS loader where to fetch that .dwarf from at runtime
    f"-sSEPARATE_DWARF_URL=file://{wasm_name}.dwarf",
    "-sSTACK_OVERFLOW_CHECK=2",
    "-sASSERTIONS=1",
    "-fsanitize=address",
    "-fsanitize=undefined",
]

# Adjust for QUICK_BUILD or DEBUG
if QUICK_BUILD:
    link_flags += ["-sERROR_ON_WASM_CHANGES_AFTER_LINK"]
elif DEBUG:
    # strip default optimization levels
    compile_flags = _remove_flags(compile_flags, ["-Oz", "-Os", "-O0", "-O1", "-O2", "-O3"])
    compile_flags += debug_compile_flags
    link_flags += debug_link_flags

# Optimize for RELEASE
if OPTIMIZED:
    compile_flags += ["-flto"]

# Handle custom export name
export_name = env.GetProjectOption("custom_wasm_export_name", "")
if export_name:
    output_js = f"{BUILD_DIR}/{export_name}.js"
    link_flags += [
        "-s", "MODULARIZE=1",
        "-s", f"EXPORT_NAME='{export_name}'",
        "-o", output_js,
    ]
    if DEBUG:
        link_flags.append(f"--source-map-base=file://{BUILD_DIR}/")

# Append flags to environment
env.Append(CCFLAGS=compile_flags)
env.Append(CXXFLAGS=compile_flags)
env.Append(LINKFLAGS=link_flags)

# FastLED library compile flags
fastled_compile_cc_flags = [
    "-Werror=bad-function-cast",
    "-Werror=cast-function-type",
    "-I/js/fastled/src/platforms/wasm/compiler",
]
fastled_compile_link_flags = [
    "--bind",
    "-Wl,--whole-archive,-fuse-ld=lld",
    "-Werror=bad-function-cast",
    "-Werror=cast-function-type",
]

if not DEBUG:
    fastled_compile_cc_flags.append(build_mode)
else:
    fastled_compile_cc_flags = _remove_flags(
        fastled_compile_cc_flags,
        ["-Oz", "-Os", "-O0", "-O1", "-O2", "-O3"]
    )
    fastled_compile_cc_flags += debug_compile_flags
    fastled_compile_link_flags += debug_link_flags

# Apply to library builders
for lb in env.GetLibBuilders():
    lb.env.Replace(CC=CC, CXX=CXX, LINK=LINK, AR="emar", RANLIB="emranlib")
    lb.env.Append(CCFLAGS=fastled_compile_cc_flags)
    lb.env.Append(LINKFLAGS=fastled_compile_link_flags)

# Banner utilities
def banner(s: str) -> str:
    lines = s.split("\n")
    widest = max(len(l) for l in lines)
    border = "#" * (widest + 4)
    out = [border]
    for l in lines:
        out.append(f"# {l:<{widest}} #")
    out.append(border)
    return "\n" + "\n".join(out) + "\n"

def print_banner(msg: str) -> None:
    print(banner(msg))

# Diagnostics
print_banner("C++/C Compiler Flags:")
print("CC/CXX flags:")
for f in compile_flags:
    print(f"  {f}")
print_banner("Linker Flags:")
for f in link_flags:
    print(f"  {f}")
print_banner("FastLED Library Flags:")
for f in fastled_compile_cc_flags:
    print(f"  {f}")
print_banner("End of Flags")
