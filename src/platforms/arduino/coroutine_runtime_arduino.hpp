#pragma once

// IWYU pragma: private

// Only compile for non-ESP32 Arduino platforms (not stub, not WASM)
#include "platforms/wasm/is_wasm.h"
#include "platforms/esp/is_esp.h"

#if defined(ARDUINO) && !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && !defined(FL_IS_ESP32)

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "fl/arduino.h"
#include "fl/singleton.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

class CoroutineRuntimeArduino : public ICoroutineRuntime {
public:
    void pumpEventQueueWithSleep(fl::u32 us) override {
        ::delayMicroseconds(us);
    }

    bool hasPlatformBackgroundEvents() override {
        return false;
    }

    void platformDelay(fl::u32 ms) override {
        ::delay((unsigned long)ms);
    }

    void platformDelayMicroseconds(fl::u32 us) override {
        ::delayMicroseconds((fl::u32)us);
    }

    fl::u32 platformMillis() override {
        return static_cast<fl::u32>(::millis());
    }

    fl::u32 platformMicros() override {
        return static_cast<fl::u32>(::micros());
    }

    void osYield() override {
        // Non-RTOS Arduino: no-op
    }
};

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeArduino>::instance();
}

}  // namespace platforms
}  // namespace fl

#endif  // ARDUINO && !FASTLED_STUB_IMPL && !FL_IS_WASM && !FL_IS_ESP32
