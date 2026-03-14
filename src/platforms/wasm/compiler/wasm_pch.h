#pragma once

// IWYU pragma: private

// ok no namespace fl

// ============================================================================
// HEADER UNIT SOURCE FOR WASM BUILDS (C++20)
// ============================================================================
// This file is compiled as a C++20 header unit (.pcm BMI) and shared across
// all WASM sketch compilations. Header units are ~2x faster than traditional
// PCH because the BMI encodes semantic info (types, templates) in compact
// binary form instead of replaying a token stream.
//
// Usage:
//   Build:  emcc -fmodule-header=user wasm_pch.h -o wasm_pch.h.pcm
//   Sketch: import "wasm_pch.h";  // in wrapper, before #include "sketch.ino"
//
// The library build (Meson) still uses this as a traditional PCH.
// Only sketch compilation uses the header unit path.
//
// Invalidation:
// The header unit BMI is automatically rebuilt when:
// - This header file content changes
// - Compiler flags change (from build_flags.toml)
// - Source fingerprint changes
// ============================================================================

// ============================================================================
// FASTLED CORE
// ============================================================================
// The main FastLED header pulls in all core FastLED functionality, types,
// and platform abstractions. This is used by virtually 100% of builds.
// FastLED.h already includes most commonly used headers, so we primarily
// need this and emscripten-specific headers.
#include "FastLED.h"  // ok include

// ============================================================================
// EMSCRIPTEN RUNTIME
// ============================================================================
// Core emscripten headers for WASM runtime support. These are used by
// platform implementation files and any code that interacts with JavaScript.
// IWYU pragma: begin_keep
#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
// IWYU pragma: end_keep

// ============================================================================
// ADDITIONAL COMMONLY USED HEADERS
// ============================================================================
// Include frequently used standard library and FastLED headers that aren't
// automatically included by FastLED.h. These headers are parsed in nearly
// every compilation unit, so precompiling them provides significant speedup.

// Standard library headers used throughout FastLED
// IWYU pragma: begin_keep
#include <cstdio>      // printf, FILE operations
// IWYU pragma: end_keep
#include "fl/stl/cstring.h"     // memcpy, memset, strlen
#include "fl/stl/cstdlib.h"     // malloc, free, abs

// FastLED utility headers commonly used across the codebase
#include "fl/stl/span.h"        // fl::span (non-owning array view)
#include "fl/stl/vector.h"      // fl::vector
#include "fl/stl/string.h"          // fl::string
#include "fl/stl/math.h"  // fl::min, fl::max, etc.
#include "fl/system/log.h"          // FL_DBG macro for debug output
#include "fl/system/log.h"         // FL_WARN macro for warnings

// ============================================================================
// FX HEADERS
// ============================================================================
// With C++20 header units, these headers are parsed once into the BMI.
// Sketches that import this header unit get fx types without re-parsing.
#include "fl/fx/fx2d.h"
#include "fl/fx/fx2d_to_1d.h"
#include "fl/fx/fx_engine.h"
#include "fl/gfx/screenmap.h"
#include "fl/fx/2d/animartrix.hpp"

// ============================================================================
// NOTES
// ============================================================================
//
// What NOT to include here:
// - Sketch-specific headers (those vary per sketch)
// - Rarely used standard library headers (increases PCH size unnecessarily)
// - Headers that change frequently (defeats PCH caching)
// - Platform headers other than emscripten (reduces portability)
//
// Maintenance:
// - Review this file periodically to ensure included headers are still common
// - Remove headers if they become rarely used
// - Add headers if build profiling shows they're parsed repeatedly
// - Keep comments updated to explain inclusion rationale
//
// ============================================================================
