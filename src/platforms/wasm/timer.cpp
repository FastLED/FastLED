#ifdef __EMSCRIPTEN__

#include <stdint.h>
#include <thread>

#include <emscripten.h>


// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

    // Replacement for 'millis' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE uint32_t millis() {
        return uint32_t(emscripten_get_now());
    }

    // Replacement for 'micros' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE uint32_t micros() {
        uint64_t out = uint64_t(emscripten_get_now() * 1000);
        return uint32_t(out & 0xFFFFFFFF);
    }

    // Replacement for 'delay' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE void delay(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

#endif  // __EMSCRIPTEN__
