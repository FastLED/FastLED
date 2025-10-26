#pragma once

/// WASM interrupt control interface
/// WASM is single-threaded and doesn't have hardware interrupts
///
/// This header exists to satisfy the platforms/interrupt.h dispatcher which
/// includes platform-specific interrupt headers based on compile-time defines.
/// Since WASM has no interrupt support, these are simple inline no-ops.

namespace fl {

/// No-op for WASM (single-threaded, no hardware interrupts)
inline void noInterrupts() {}

/// No-op for WASM (single-threaded, no hardware interrupts)
inline void interrupts() {}

}  // namespace fl
