#pragma once

// clib headers
#include <stdio.h>
#include <thread>

// emscripten headers
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>


// Provide missing functions for WebAssembly build.
extern "C" {
    // Replacement for 'micros' in WebAssembly context
    uint32_t micros() {
        return emscripten_get_now() * 1000;  // Emscripten provides milliseconds, multiply to get microseconds
    }

    // Replacement for 'millis' in WebAssembly context
    uint32_t millis() {
        return emscripten_get_now();  // Emscripten returns time in milliseconds
    }

    // Replacement for 'delay' in WebAssembly context
    void delay(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}