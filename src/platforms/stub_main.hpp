/*
 * Base implementation for stub main() that can be used with Arduino sketches.
 * This header provides the infrastructure to run .ino files as C++ programs.
 *
 * Usage: Include this header along with your .ino file to get a working main().
 */

#pragma once

#ifndef _FASTLED_STRINGIFY
#define _FASTLED_STRINGIFY_HELPER(x) #x
#define _FASTLED_STRINGIFY(x) _FASTLED_STRINGIFY_HELPER(x)
#endif

// Include function.h and time_stub.h at file scope
#include "fl/compiler_control.h"  // For FL_MAYBE_UNUSED
#include "fl/stl/function.h"
#if !defined(ARDUINO) || defined(FASTLED_USE_STUB_ARDUINO)
#include "platforms/stub/time_stub.h"
#endif

extern void init();
extern void setup();
extern void loop();

// Note: The .ino file should be included BEFORE this file provides main()
// The .ino file provides: void setup() and void loop()

// Helper function to determine if loop should continue
// When FASTLED_STUB_MAIN_FAST_EXIT defined: runs 5 iterations then stops
// When undefined: runs forever
namespace fl {
namespace stub_main {

bool keep_going() {
#ifndef FASTLED_STUB_MAIN_FAST_EXIT
    return true;
#else
    static int max_iterations = 5; // okay static in header
    bool keep_going = max_iterations-- > 0;
    return keep_going;
#endif
}

// Helper function to set delay to zero for fast testing
// Only sets delay to zero when FASTLED_STUB_MAIN_FAST_EXIT is defined
static inline void maybe_set_delay_to_zero() {
#if defined(FASTLED_STUB_MAIN_FAST_EXIT) && (!defined(ARDUINO) || defined(FASTLED_USE_STUB_ARDUINO))
    setDelayFunction([](uint32_t ms) {
        (void)ms; // Suppress unused parameter warning
    });
#endif
}

void setup() {
    maybe_set_delay_to_zero();
    ::init();
    ::setup();
}

bool next_loop() {
    loop();
    delay(0);  // Needed for watchdog timers not to fail
    return keep_going();
}

}  // namespace stub_main
}  // namespace fl

// Only provide main() for non-Arduino platforms (stub platforms)
// Real Arduino platforms (ESP32, etc.) have their own main() in the framework
#if !defined(ARDUINO) || defined(FASTLED_USE_STUB_ARDUINO)
int main() {
    fl::stub_main::setup();
    while (fl::stub_main::next_loop()) {;}
    return 0;
}
#endif
