// IWYU pragma: private

/// @file platforms/arm/rp/rp2040/delay.cpp
/// RP2040 platform-specific delay implementation

// ok no namespace fl
#include "platforms/cycle_type.h"
#include "platforms/arm/rp/is_rp.h"

#ifdef FL_IS_RP2040
#include "fl/system/arduino.h"

#if defined(ARDUINO_ARCH_MBED)
// IWYU pragma: begin_keep
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "platform/mbed_wait_api.h" // ok include path
// IWYU pragma: end_keep
#endif

void delay_cycles_pico(fl::u32 cycles) {
#if defined(ARDUINO_ARCH_MBED)
  const fl::u32 hz = clock_get_hz(clk_sys);
  if (!cycles || !hz) return;
  const fl::u64 ns = (static_cast<fl::u64>(cycles) * 1000000000ull + hz - 1) / hz;
  if (ns > 10000) {
    busy_wait_us((ns + 999) / 1000);
  } else {
    wait_ns(static_cast<fl::u32>(ns));
  }
#else
  busy_wait_at_least_cycles(cycles);
#endif
}
#endif  // FL_IS_RP2040
