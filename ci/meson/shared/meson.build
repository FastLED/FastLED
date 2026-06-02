# Shared compiler flags consumed by native builds.
# NOTE: WASM builds read flags from src/platforms/wasm/compiler/build_flags.toml
# via ci/wasm_flags.py. If adding flags here that should also apply to WASM,
# add them to build_flags.toml too.
shared_no_exception_flags = ['-fno-exceptions', '-fno-rtti']

# Shared preprocessor defines
shared_defines = ['-DFASTLED_USE_PROGMEM=0']

# ============================================================================
# PATH NORMALIZATION (Windows #pragma once correctness) — issue #2328
# ============================================================================
# On Windows, Meson's include_directories() propagates -I flags using native
# backslash + relative-path spellings (e.g. -I..\..\src\platforms\stub). Clang
# keys #pragma once deduplication on the raw resolved path string — if the
# same directory reaches a translation unit via two different spellings
# (forward slash vs backslash, absolute vs relative, with/without /./), the
# header appears to be "different files" and #pragma once is silently defeated,
# producing double-definition errors (see #2324, #2325).
#
# The canonical path_norm(p) below returns an absolute, forward-slash-only
# form of p. Every -I flag we emit from our Meson files MUST be built via
# path_norm() so that the exact same string is passed at every call site.
#
# HOW TO USE (Meson has no user-defined functions, so we stamp a canonical
# forward-slash project root and do string concat):
#
#   abs_path = path_norm_root / 'src'          # absolute forward-slash
#   cpp_args += ['-I' + abs_path]
#
# For convenience, canonical -I arg lists for the four project-wide include
# roots are exported as path_norm_I_root, path_norm_I_src, path_norm_I_tests,
# path_norm_I_stub. All consumer meson.build files should pull these from
# here instead of redeclaring -I flags locally.
# ============================================================================

# meson.project_source_root() on Windows returns native backslashes
# (e.g. C:\Users\...). Normalize it once and compose every derived path via
# literal '/' string concatenation so paths are absolute + forward-slash +
# spelled identically everywhere.
#
# Why NOT meson's `/` path-join operator: on Windows it emits the native
# separator (backslash) regardless of how its operands are spelled. So
# `path_norm_root / 'src'` ends up as `C:/Users/.../fastled3\src` — a
# single backslash injected mid-path. clang/zccache pass that `-I` flag
# through verbatim, then when `#include "fl/stl/cstdio.h"` resolves
# against it the resulting include path becomes `C:/.../fastled3\src/fl/
# stl/cstdio.h` with mixed separators. The PCH canonicalizes one slash
# spelling; the test TU sees another; `#pragma once` fails to dedupe and
# the user sees `error: redefinition of 'LogLevel'`. Reproduced and root-
# caused on 2026-06-02 against `bash test --cpp` after merging #2683 and
# #2684. Literal `+ '/' +` concatenation keeps everything forward-slash.
path_norm_root = meson.project_source_root().replace('\\', '/')

# Canonical absolute forward-slash -I args for the four project-wide
# include roots. Use these everywhere instead of redeclaring.
path_norm_I_root   = '-I' + path_norm_root
path_norm_I_src    = '-I' + path_norm_root + '/src'
path_norm_I_tests  = '-I' + path_norm_root + '/tests'
path_norm_I_stub   = '-I' + path_norm_root + '/src/platforms/stub'
