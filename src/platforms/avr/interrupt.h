#pragma once

/// AVR-specific interrupt control interface
/// Minimal bindings to cli() and sei() instructions

namespace fl {

/// Disable interrupts on AVR
void noInterrupts();

/// Enable interrupts on AVR
void interrupts();

}  // namespace fl
