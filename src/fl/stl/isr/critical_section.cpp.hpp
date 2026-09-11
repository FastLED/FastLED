// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

/// @file fl/stl/isr/critical_section.cpp.hpp
/// @brief critical_section implementation

#include "fl/stl/isr/critical_section.h"
#include "platforms/isr.h"
#include "fl/stl/noexcept.h"

namespace fl {

void interrupt_disable() {
    interruptsDisable();
}

void interrupt_enable() {
    interruptsEnable();
}

namespace isr {

critical_section::critical_section() {
    fl::interrupt_disable();
}

critical_section::~critical_section() FL_NO_EXCEPT {
    fl::interrupt_enable();
}

} // namespace isr
} // namespace fl
