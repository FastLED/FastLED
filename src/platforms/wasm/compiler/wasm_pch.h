#pragma once

// ok no namespace fl

// ============================================================================
// PRECOMPILED HEADER FOR WASM BUILDS
// ============================================================================
// This file is compiled once and shared across all WASM compilation units
// (library and sketches) to significantly speed up compilation. It includes
// commonly used headers that rarely change.
//
// Key Design Principles:
// - Include only headers used by >80% of compilation units
// - Keep PCH size reasonable (target < 50MB)
// - Avoid platform-specific headers except WASM itself
// - Must be compatible with all build modes (debug, release, etc.)
//
// PCH Invalidation:
// This PCH is automatically rebuilt when:
// - This header file content changes
// - Compiler flags change (from build_flags.toml)
// - Emscripten version changes
//
// Performance Impact:
// - PCH build time: ~3 seconds (one-time cost)
// - Incremental build speedup: 80%+ for sketch-only changes
// - Saves ~986 header dependencies from being reparsed each build
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
#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

// ============================================================================
// ADDITIONAL COMMONLY USED HEADERS
// ============================================================================
// Include frequently used standard library and FastLED headers that aren't
// automatically included by FastLED.h. These headers are parsed in nearly
// every compilation unit, so precompiling them provides significant speedup.

// Standard library headers used throughout FastLED
#include <cstdio>      // printf, FILE operations
#include "fl/stl/cstring.h"     // memcpy, memset, strlen
#include "fl/stl/cstdlib.h"     // malloc, free, abs

// FastLED utility headers commonly used across the codebase
#include "fl/stl/span.h"        // fl::span (non-owning array view)
#include "fl/stl/vector.h"      // fl::vector
#include "fl/str.h"          // fl::string
#include "fl/math_macros.h"  // fl::min, fl::max, etc.
#include "fl/dbg.h"          // FL_DBG macro for debug output
#include "fl/warn.h"         // FL_WARN macro for warnings

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
