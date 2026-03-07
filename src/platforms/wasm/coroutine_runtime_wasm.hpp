#pragma once

// IWYU pragma: private

// Only compile for WASM platform
#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "fl/singleton.h"
#include <emscripten.h>
// IWYU pragma: end_keep

// Forward declare WASM timer functions (defined in timer.cpp.hpp)
extern "C" {
    fl::u32 millis();
    fl::u32 micros();
}

namespace fl {
namespace platforms {

class CoroutineRuntimeWasm : public ICoroutineRuntime {
public:
    void pumpEventQueueWithSleep(fl::u32 us) override {
        // WASM with PROXY_TO_PTHREAD runs on a real pthread, so we can use
        // real OS sleep to yield CPU instead of busy-waiting
        if (us == 0) return;
        emscripten_thread_sleep(us / 1000.0);  // Takes milliseconds (double)
    }

    bool hasPlatformBackgroundEvents() override {
        return false;
    }

    void platformDelay(fl::u32 ms) override {
        if (ms == 0) return;
        emscripten_thread_sleep(ms);
    }

    void platformDelayMicroseconds(fl::u32 us) override {
        if (us == 0) return;
        emscripten_thread_sleep(us / 1000.0);
    }

    fl::u32 platformMillis() override {
        return ::millis();
    }

    fl::u32 platformMicros() override {
        return ::micros();
    }

    void osYield() override {
        // WASM: no-op (OS handles scheduling)
    }
};

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeWasm>::instance();
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_WASM
