// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once
#include "fl/stl/noexcept.h"

namespace fl {

inline void print_arduino(const char* str) FL_NO_EXCEPT {
    if (!str) return;
    
    // Generic Arduino platforms and final fallback
    // Only use Serial if Arduino.h has been included and Serial is available
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.print(str);
    }
    #endif
    // If no Serial available, output goes nowhere (silent)
    // This prevents crashes on platforms where Serial isn't initialized
}

inline void println_arduino(const char* str) FL_NO_EXCEPT {
    if (!str) return;
    print_arduino(str);
    print_arduino("\n");
}

} // namespace fl
