/// WASM interrupt control implementation

#ifdef __EMSCRIPTEN__

#include "platforms/wasm/interrupt.h"

namespace fl {

void noInterrupts() {
    // No-op: WASM is single-threaded
}

void interrupts() {
    // No-op: WASM is single-threaded
}

}  // namespace fl

#endif  // __EMSCRIPTEN__
