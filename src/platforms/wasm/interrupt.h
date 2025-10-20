#pragma once

/// WASM interrupt control interface
/// WASM is single-threaded and doesn't have hardware interrupts - no-op implementation

namespace fl {

/// No-op for WASM
void noInterrupts();

/// No-op for WASM
void interrupts();

}  // namespace fl
