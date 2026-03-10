#pragma once

// IWYU pragma: private

// Only compile for non-ESP32 Arduino platforms (not stub, not WASM)
#include "platforms/wasm/is_wasm.h"
#include "platforms/esp/is_esp.h"

#if defined(ARDUINO) && !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && !defined(FL_IS_ESP32)

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "fl/singleton.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

class CoroutineRuntimeArduino : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) override {
        // Generic Arduino: no background coroutines, nothing to pump.
        (void)us;
    }
};

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeArduino>::instance();
}

}  // namespace platforms
}  // namespace fl

#endif  // ARDUINO && !FASTLED_STUB_IMPL && !FL_IS_WASM && !FL_IS_ESP32
