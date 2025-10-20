#pragma once

/// @file interrupt_control.h
/// Cross-platform interrupt enable/disable functionality
///
/// Minimal platform bindings for interrupt control.
///
/// Usage:
/// @code
///     fl::noInterrupts();   // Disable interrupts
///     // critical code
///     fl::interrupts();     // Enable interrupts
/// @endcode

namespace fl {

/// Disable interrupts
void noInterrupts();

/// Enable interrupts
void interrupts();

}  // namespace fl
