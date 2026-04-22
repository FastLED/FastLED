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

# ============================================================================
# WASM static library (libfastled.a)
# ============================================================================
# All sources (including platforms/shared) are compiled together for WASM.
# The weak/strong symbol separation is a native build concern only.
wasm_all_sources = fastled_sources + platforms_shared_files

# Absolute forward-slash include paths (issue #2328). Meson's
# include_directories() emits *relative* -I flags with backslashes on Windows,
# which causes clang/emscripten to treat the same header as distinct files
# for #pragma once purposes. path_norm_* are exported from
# ci/meson/shared/meson.build.
wasm_abs_include_args = [
  path_norm_I_src,
  '-I' + path_norm_root / 'src/platforms/wasm/compiler',
]

fastled_wasm_lib = static_library('fastled',
  wasm_all_sources,
  cpp_args: wasm_abs_include_args + wasm_lib_compile_args,
  pic: false,
  implicit_include_directories: false,
  install: false
)

message('WASM build configured: libfastled.a target available')
message('Build mode: ' + build_mode)
