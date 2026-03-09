#pragma once

// IWYU pragma: private

// Only compile for WASM platform
#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "fl/singleton.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

class CoroutineRuntimeWasm : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) override {
        // WASM: no FastLED background coroutines, nothing to pump.
        // Browser handles its own event loop scheduling.
        (void)us;
    }
};

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeWasm>::instance();
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_WASM
