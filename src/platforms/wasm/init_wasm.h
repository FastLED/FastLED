// ok no namespace fl
// allow-include-after-namespace
#pragma once
#include "fl/stl/noexcept.h"

// IWYU pragma: private

/// @file platforms/wasm/init_wasm.h
/// @brief WebAssembly platform initialization
///
/// WASM platform requires explicit initialization of the driver listener system
/// that tracks strip additions, frame events, and canvas UI state.

namespace fl {
namespace platforms {

/// @brief Initialize WebAssembly platform
///
/// Performs one-time initialization of WASM-specific subsystems:
/// - Engine Listener: Tracks strip additions, frame end events, and canvas UI updates
///
/// The driver listener system provides integration between FastLED and the
/// JavaScript runtime environment. Calling Init() explicitly ensures this
/// system is ready before any LED operations begin.
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// @note Implementation is in src/platforms/wasm/init_wasm.cpp
void init() FL_NOEXCEPT;

} // namespace platforms
} // namespace fl
