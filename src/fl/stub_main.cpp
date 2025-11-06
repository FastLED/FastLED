/*
 * This is a stub implementation of main that can be used to include an *.ino
 * file which is so close to C++ that many of them can be compiled as C++. The
 * notable difference between a *.ino file and a *.cpp file is that the *.ino
 * file does not need to include function prototypes, and are instead
 * auto-generated.
 */

// This can't be in the namespace fl. It needs to be in the global namespace.

#if defined(FASTLED_STUB_MAIN) || defined(FASTLED_STUB_MAIN_INCLUDE_INO)

#ifndef _FASTLED_STRINGIFY
#define _FASTLED_STRINGIFY_HELPER(x) #x
#define _FASTLED_STRINGIFY(x) _FASTLED_STRINGIFY_HELPER(x)
#endif

// Include function.h and time_stub.h at file scope (not inside function)
#include "fl/function.h"
#include "platforms/stub/time_stub.h"

#ifdef FASTLED_STUB_MAIN_INCLUDE_INO
// Correctly include the file by expanding and stringifying the macro value
#include _FASTLED_STRINGIFY(FASTLED_STUB_MAIN_INCLUDE_INO)
#else
void setup() {}
void loop() {}
#endif // FASTLED_STUB_MAIN_INCLUDE_INO



// Fast test mode: run setup() once and loop() for limited iterations
static int main_stub() {
    // Override delay function to return immediately for fast testing
    setDelayFunction([](uint32_t ms) {
        // Fast delay override - do nothing for speed
        (void)ms; // Suppress unused parameter warning
    });

    init();   // Arduino init() function
    setup();  // User-defined setup()

    // For testing, run loop() limited times instead of infinitely
    // This allows examples to complete successfully in test environments
    const int max_iterations = 5;
    for (int i = 0; i < max_iterations; i++) {
        loop();
    }
    return 0;
}

// Production mode: run setup() once and loop() infinitely
static int main_example() {
    init();   // Arduino init() function
    setup();  // User-defined setup()

    // Run loop() infinitely (normal Arduino behavior)
    while (1) {
        loop();
        delay(0);  // Needed for watchdog timers not to fail
    }
}


int main() {
#ifdef FASTLED_STUB_IMPL
    return main_stub();
#else
    return main_example();
#endif
}
#endif // FASTLED_STUB_MAIN_INCLUDE_INO
