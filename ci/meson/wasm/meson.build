# ============================================================================
# WASM BUILD CONFIGURATION (Emscripten cross-compilation)
# ============================================================================
# When cross-compiling for WASM via emscripten, we use completely different
# compiler flags, link flags, and build targets. Source discovery is shared
# with the native build (in root meson.build), but everything else is
# WASM-specific.
#
# Flags are migrated from src/platforms/wasm/compiler/build_flags.toml
# to keep all build configuration in one place (meson.build).
#
# Variables inherited from root meson.build:
#   build_mode, fastled_sources, platforms_shared_files,
#   src_dir, uv_prog
# ============================================================================

# WASM compiler include directories
# Path is relative to this meson.build (ci/meson/wasm/)
wasm_compiler_dir = include_directories('../../../src/platforms/wasm/compiler')

# ============================================================================
# WASM preprocessor defines (from build_flags.toml [all].defines)
# ============================================================================
wasm_defines = [
  # FastLED Engine Configuration
  '-DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50',
  '-DFASTLED_FORCE_NAMESPACE=1',
  '-DFASTLED_USE_PROGMEM=0',
  '-DUSE_OFFSET_CONVERTER=0',
  '-DGL_ENABLE_GET_PROC_ADDRESS=0',

  # Threading and Concurrency Support
  '-D_REENTRANT=1',

  # Emscripten-Specific Configuration
  '-DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0',
]

# ============================================================================
# WASM compiler flags (from build_flags.toml [all].compiler_flags)
# ============================================================================
wasm_common_flags = [
  # Language Standard
  '-std=gnu++17',
  '-fpermissive',

  # Warning Configuration
  '-Wno-constant-logical-operand',
  '-Wnon-c-typedef-for-linkage',
  '-Werror=bad-function-cast',
  '-Werror=cast-function-type',

  # Runtime Behavior
  '-fno-threadsafe-statics',
  '-fno-exceptions',
  '-fno-rtti',

  # Threading Support
  '-pthread',

  # PCH Support
  '-fpch-instantiate-templates',
]

# ============================================================================
# WASM library-specific flags (from build_flags.toml [library])
# ============================================================================
wasm_library_flags = [
  '-flto=thin',
  '-Wall',
]

# ============================================================================
# WASM sketch-specific defines (from build_flags.toml [sketch])
# ============================================================================
wasm_sketch_defines = [
  '-DSKETCH_COMPILE=1',
  '-DFASTLED_WASM_USE_CCALL',
]

# ============================================================================
# WASM mode-specific flags (from build_flags.toml [build_modes.*])
# ============================================================================
if build_mode == 'debug'
  wasm_mode_flags = [
    '-g3',
    '-gsource-map',
    '-fno-inline',
    '-O0',
  ]
  wasm_mode_link_flags = [
    '--profiling-funcs',
    '-sSEPARATE_DWARF_URL=fastled.wasm.dwarf',
    '-sSTACK_OVERFLOW_CHECK=2',
    '-sASSERTIONS=1',
  ]
elif build_mode == 'quick'
  wasm_mode_flags = [
    '-flto=thin',
    '-O1',
    '-g0',
    '-fno-inline-functions',
    '-fno-vectorize',
    '-fno-unroll-loops',
    '-fno-strict-aliasing',
    '-fno-delayed-template-parsing',
    '-fmax-type-align=4',
    '-ffast-math',
    '-fno-finite-math-only',
    '-fno-math-errno',
    '-fno-exceptions',
    '-fno-rtti',
  ]
  wasm_mode_link_flags = [
    '-Wl,--thinlto-cache-dir=build/wasm/lto_cache',
    '--profiling-funcs',
    '--emit-symbol-map',
    '-sINITIAL_MEMORY=67108864',
    '-sASSERTIONS=0',
    '-sSTACK_OVERFLOW_CHECK=0',
    '-sWASM_BIGINT=0',
    '-sERROR_ON_UNDEFINED_SYMBOLS=1',
  ]
elif build_mode == 'release'
  wasm_mode_flags = [
    '-Oz',
  ]
  wasm_mode_link_flags = []
else
  error('Invalid build_mode: @0@. Must be one of: debug, quick, release'.format(build_mode))
endif

# ============================================================================
# WASM base linking flags (from build_flags.toml [linking.base])
# ============================================================================
wasm_base_link_flags = [
  '-sWASM=1',
  '-fuse-ld=wasm-ld',
  '-pthread',
  '-sUSE_PTHREADS=1',
  '-sPROXY_TO_PTHREAD',
]

# ============================================================================
# WASM sketch linking flags (from build_flags.toml [linking.sketch])
# ============================================================================
wasm_sketch_link_flags = [
  '-sMODULARIZE=1',
  '-sEXPORT_NAME=fastled',
  '-sALLOW_MEMORY_GROWTH=1',
  '-sINITIAL_MEMORY=134217728',
  '-sAUTO_NATIVE_LIBRARIES=0',
  '-sEXPORTED_RUNTIME_METHODS=[\'ccall\',\'cwrap\',\'stringToUTF8\',\'UTF8ToString\',\'lengthBytesUTF8\',\'HEAPU8\',\'getValue\']',
  '-sEXPORTED_FUNCTIONS=[\'_malloc\',\'_free\',\'_main\',\'_extern_setup\',\'_extern_loop\',\'_fastled_declare_files\',\'_getStripPixelData\',\'_getFrameData\',\'_getScreenMapData\',\'_freeFrameData\',\'_getFrameVersion\',\'_hasNewFrameData\',\'_js_fetch_success_callback\',\'_js_fetch_error_callback\',\'_pushAudioSamples\']',
  '-sEXIT_RUNTIME=0',
  '-sFILESYSTEM=0',
  '-Wl,--strip-debug',
  '-Wl,--no-export-dynamic',
]

# ============================================================================
# Assemble final WASM compilation args
# ============================================================================
wasm_lib_compile_args = wasm_defines + wasm_common_flags + wasm_library_flags + wasm_mode_flags
wasm_sketch_compile_args = wasm_defines + wasm_common_flags + wasm_sketch_defines + wasm_mode_flags
wasm_link_args = wasm_base_link_flags + wasm_sketch_link_flags + wasm_mode_link_flags

# ============================================================================
# WASM PCH (Precompiled Header)
# ============================================================================
# Uses the same custom_target PCH pattern as the native build, but with the
# WASM-specific PCH header and emscripten compiler.
compile_pch_script = meson.project_source_root() / 'ci' / 'compile_pch.py'
wasm_pch_header = meson.project_source_root() / 'src' / 'platforms' / 'wasm' / 'compiler' / 'wasm_pch.h'

wasm_pch = custom_target('wasm_pch',
  input: meson.project_source_root() / 'src' / 'platforms' / 'wasm' / 'compiler' / 'wasm_pch.h',
  output: 'wasm_pch.h.pch',
  depfile: 'wasm_pch.h.d',
  command: [
    uv_prog, 'run', 'python', compile_pch_script,
    'clang-tool-chain-emcc',
    '-x', 'c++-header',
    '@INPUT@',
    '-o', '@OUTPUT@',
    '-MD', '-MF', '@DEPFILE@',
    wasm_lib_compile_args,
    '-I' + meson.project_source_root() / 'src',
    '-I' + meson.project_source_root() / 'src' / 'platforms' / 'wasm' / 'compiler',
    '-Werror=invalid-pch',
    '-verify-pch',
    '-fpch-validate-input-files-content',
    '-fpch-instantiate-templates',
  ]
)

# ============================================================================
# WASM static library (libfastled.a)
# ============================================================================
# All sources (including platforms/shared) are compiled together for WASM.
# The weak/strong symbol separation is a native build concern only.
wasm_all_sources = fastled_sources + platforms_shared_files

wasm_pch_usage_args = [
  '-include-pch', meson.current_build_dir() / 'wasm_pch.h.pch',
  '-Werror=invalid-pch',
  '-fpch-validate-input-files-content',
]

fastled_wasm_lib = static_library('fastled',
  wasm_all_sources + [wasm_pch],
  include_directories: [src_dir, wasm_compiler_dir],
  cpp_args: wasm_lib_compile_args + wasm_pch_usage_args,
  install: false
)

message('WASM build configured: libfastled.a target available')
message('Build mode: ' + build_mode)
