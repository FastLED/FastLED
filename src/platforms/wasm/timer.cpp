#ifdef __EMSCRIPTEN__

#include <stdint.h>
#include <thread>

#include <emscripten.h>


// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

    // Replacement for 'millis' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE uint32_t millis() {
        return emscripten_get_now();
    }

    // Replacement for 'micros' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE uint32_t micros() {
        return millis() * 1000;
    }

    // Replacement for 'delay' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE void delay(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

#endif  // __EMSCRIPTEN__
