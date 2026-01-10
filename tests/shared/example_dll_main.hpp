/*
 * DLL entry point for Arduino examples.
 * Exports run_example() function that can be called by example_runner.exe
 */

#pragma once

// ok no namespace fl

#include "platforms/stub_main.hpp"

// Export function for DLL mode - called by example_runner.exe
extern "C" {
#ifdef _WIN32
    __declspec(dllexport)
#else
    __attribute__((visibility("default")))
#endif
    int run_example(int argc, const char** argv) {
        (void)argc;  // Suppress unused parameter warning
        (void)argv;  // Examples don't typically use command-line args

        // Run setup() once, then loop() until next_loop() returns false
        fl::stub_main::setup();
        while (fl::stub_main::next_loop()) {
            // Loop continues based on FASTLED_STUB_MAIN_FAST_EXIT
        }

        return 0;
    }
}
