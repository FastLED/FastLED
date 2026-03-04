# ============================================================================
# WASM BUILD CONFIGURATION (Emscripten cross-compilation)
# ============================================================================
# When cross-compiling for WASM via emscripten, we use completely different
# compiler flags, link flags, and build targets. Source discovery is shared
# with the native build (in root meson.build), but everything else is
# WASM-specific.
#
# Flags are loaded at configure time from src/platforms/wasm/compiler/build_flags.toml
# via ci/wasm_flags.py to keep a single source of truth for all build consumers.
#
# Variables inherited from root meson.build:
#   build_mode, fastled_sources, platforms_shared_files,
#   src_dir, uv_prog
# ============================================================================

# WASM compiler include directories
# Path is relative to this meson.build (ci/meson/wasm/)
wasm_compiler_dir = include_directories('../../../src/platforms/wasm/compiler')

# ============================================================================
# WASM flags loaded from build_flags.toml via ci/wasm_flags.py
# ============================================================================
# All WASM compilation flags are defined in:
#   src/platforms/wasm/compiler/build_flags.toml
# This single source of truth is read by ci/wasm_flags.py and shared by
# meson, wasm_build.py, wasm_build_library.py, wasm_compile_pch.py, and
# wasm_compile_native.py to prevent flag drift.
# ============================================================================
wasm_flags_script = meson.project_source_root() / 'ci' / 'wasm_flags.py'

lib_result = run_command(uv_prog, 'run', 'python', wasm_flags_script,
  '--target', 'lib_compile', '--mode', build_mode, check: true)
wasm_lib_compile_args = lib_result.stdout().strip().split('\n')

sketch_result = run_command(uv_prog, 'run', 'python', wasm_flags_script,
  '--target', 'sketch_compile', '--mode', build_mode, check: true)
wasm_sketch_compile_args = sketch_result.stdout().strip().split('\n')

link_result = run_command(uv_prog, 'run', 'python', wasm_flags_script,
  '--target', 'link', '--mode', build_mode, check: true)
wasm_link_args = link_result.stdout().strip().split('\n')

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
    # Meson-injected defaults that must match static_library() compilation:
    '-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST',
    '-D_FILE_OFFSET_BITS=64',
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

# Absolute include paths — must match the PCH compilation so that #pragma once
# recognises headers included via the PCH and via source files as the same file.
# Meson's include_directories() emits *relative* -I flags which causes clang
# (especially emscripten on Windows) to treat them as distinct files.
wasm_abs_include_args = [
  '-I' + meson.project_source_root() / 'src',
  '-I' + meson.project_source_root() / 'src' / 'platforms' / 'wasm' / 'compiler',
]

wasm_pch_usage_args = [
  '-include-pch', meson.current_build_dir() / 'wasm_pch.h.pch',
  '-Werror=invalid-pch',
  '-fpch-validate-input-files-content',
]

fastled_wasm_lib = static_library('fastled',
  wasm_all_sources + [wasm_pch],
  cpp_args: wasm_abs_include_args + wasm_lib_compile_args + wasm_pch_usage_args,
  pic: false,
  install: false
)

message('WASM build configured: libfastled.a target available')
message('Build mode: ' + build_mode)
