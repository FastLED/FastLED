/// @file platforms/arm/rp/rp2040/delay.cpp
/// RP2040 platform-specific delay implementation

// ok no namespace fl
#include "platforms/cycle_type.h"

#ifdef ARDUINO_ARCH_RP2040
#include "Arduino.h"  // Include Arduino.h here for busy_wait_at_least_cycles

/// RP2040: Pico SDK provides busy_wait_at_least_cycles as a static inline in pico/platform.h
/// This function wraps it for use in delay operations
void delay_cycles_pico(fl::u32 cycles) {
  busy_wait_at_least_cycles(cycles);
}
#endif  // ARDUINO_ARCH_RP2040
