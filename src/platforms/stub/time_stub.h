// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

// Stub timing functions for Arduino compatibility
// These provide timing functionality when using the stub platform
// Only declare these when NOT on a real Arduino platform (exclude real Arduino, not stub Arduino)

#if !defined(ARDUINO) || defined(FASTLED_USE_STUB_ARDUINO)
extern "C" {
    // Global timing functions for Arduino compatibility
    // These are provided by the platform layer but need to be declared globally
    fl::u32 millis(void) FL_NO_EXCEPT;
    fl::u32 micros(void) FL_NO_EXCEPT;
    void yield(void) FL_NO_EXCEPT;
}

// C++ function to override delay behavior for fast testing
void setDelayFunction(const fl::function<void(fl::u32)>& delayFunc) FL_NO_EXCEPT;

// Clear the delay override (must be called before unloading DLLs that set it)
void clearDelayFunction() FL_NO_EXCEPT;

// Check if delay override is active (for fast testing)
bool isDelayOverrideActive(void) FL_NO_EXCEPT;

// ---------------------------------------------------------------------------
// Clock override (for deterministic timing tests)
// ---------------------------------------------------------------------------
//
// The stub derives millis(), micros(), and therefore fl::chrono::steady_clock
// from one root, fl::platforms::micros64(). Overriding that root is enough to
// put a test in full control of host time — there is no second clock to keep
// in sync, and the three views can never disagree.
//
// This is what timing tests should use instead of sleeping. `sleep_for` only
// guarantees a LOWER bound, so any assertion that depends on time NOT having
// passed is a bet on the host scheduler; under CI load that bet loses (see
// FastLED#3978). With a frozen clock, elapsed time is whatever the test says.
//
// The installed function is called from every thread that asks for the time,
// so it must be safe to invoke concurrently. Always clear it when done —
// prefer an RAII guard, so a failing assertion cannot leak a frozen clock into
// the rest of the binary.
//
// Note `fl::inject_time_provider()` is a different, narrower seam: it replaces
// fl::millis() only and leaves micros()/steady_clock on real time.
void setClockFunction(fl::u64 (*micros64Func)()) FL_NO_EXCEPT;

// Restore the real host clock.
void clearClockFunction(void) FL_NO_EXCEPT;

// True while a clock override is installed.
bool isClockOverrideActive(void) FL_NO_EXCEPT;
#endif  // !ARDUINO || FASTLED_USE_STUB_ARDUINO
