
Import("env", "projenv")

# projenv is used for compiling individual files, env for linking
# libraries have their own env in env.GetLibBuilders()

# this is kinda hacky but let's just replace the default toolchain
# with emscripten. the right way to do this would be to create a
# SCons toolchain file for emscripten, and platformio platform for
# WebAssembly, but this is easier for now

projenv.Replace(CC="emcc", CXX="em++", LINK="em++", AR="emar", RANLIB="emranlib")

env.Replace(CC="emcc", CXX="em++", LINK="em++", AR="emar", RANLIB="emranlib")

wasmflags = [
    "-s",
    "EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']",
    "-s",
    "ALLOW_MEMORY_GROWTH=1",
    "-s",
    "EXPORTED_FUNCTIONS=['_malloc', '_free', '_main']",
    "-s",
    "INITIAL_MEMORY=1073741824",
    "-s",
    "STACK_SIZE=536870912",
    "-s",
    "ENVIRONMENT=web",
    "--shell-file",
    "shell.html"
]

export_name = env.GetProjectOption("custom_wasm_export_name", "")
if export_name:
    wasmflags += [
        "-s",
        "MODULARIZE=1",
        "-s",
        f"EXPORT_NAME='{export_name}'",
        "-o",
        f"{env.subst('$BUILD_DIR')}/{export_name}.html",
    ]

env.Append(LINKFLAGS=wasmflags)


# Pass flags to the other Project Dependencies (libraries)
for lb in env.GetLibBuilders():
    lb.env.Replace(CC="emcc", CXX="em++", LINK="em++", AR="emar", RANLIB="emranlib")


