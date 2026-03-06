/// @file fl/stl/isr/critical_section.h
/// @brief RAII critical section helper and interrupt control declarations

#pragma once

namespace fl {

/// Disable global interrupts (platform-specific).
void interrupt_disable();

/// Enable global interrupts (platform-specific).
void interrupt_enable();

namespace isr {

/// @brief RAII helper for critical sections (interrupt disable/enable)
/// Automatically disables interrupts on construction and enables on destruction.
/// Use this for protecting shared data accessed from both ISR and main context.
///
/// Example:
///   {
///       critical_section cs;  // Interrupts disabled here
///       shared_data = new_value;
///   }  // Interrupts automatically re-enabled here
class critical_section {
public:
    critical_section();
    ~critical_section();
    critical_section(const critical_section&) = delete;
    critical_section& operator=(const critical_section&) = delete;
};

// Backward-compatible type alias
using CriticalSection = critical_section;

} // namespace isr
} // namespace fl
