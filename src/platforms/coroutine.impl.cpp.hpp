// ok no namespace fl

// IWYU pragma: private

/// @file platforms/coroutine.impl.cpp.hpp
/// @brief Platform dispatch for coroutine implementations
///
/// This is the SINGLE file that dispatches to platform-specific coroutine
/// implementations. Each platform implementation provides:
///   - ICoroutineRuntime::instance()
///   - createTaskCoroutine()
///   - ICoroutineTask::exitCurrent()
///
/// Supported platforms:
/// - WASM: JSPI-based cooperative coroutines via coroutine_runtime_wasm.impl.hpp
/// - Stub/Host: std::thread implementation via coroutine_stub.impl.hpp (for testing)
/// - ESP32: FreeRTOS tasks via coroutine_esp32.impl.hpp
/// - Teensy: Cooperative ARM context switching via coroutine_teensy.impl.hpp
/// - Arduino (non-ESP32): No-op runtime + null task coroutine
/// - Other: Null implementation (no-op)

// Platform detection
#include "platforms/arm/is_arm.h"
#include "platforms/arm/teensy/is_teensy.h"
#include "platforms/esp/is_esp.h"
#include "platforms/wasm/is_wasm.h"

#if defined(FL_IS_WASM)
// WASM: JSPI-based cooperative coroutines
#include "platforms/wasm/coroutine_runtime_wasm.impl.hpp"
#elif defined(FASTLED_STUB_IMPL)
// Stub/Host: Thread-based coroutine runner
// Must check before ARDUINO because stub builds also define ARDUINO=10808
#include "platforms/stub/coroutine_stub.impl.hpp"
#elif defined(FL_IS_ESP32)
// ESP32: FreeRTOS task implementation
#include "platforms/esp/32/coroutine_esp32.impl.hpp"
#elif defined(FL_IS_TEENSY_4X)
// Teensy: Cooperative context switching on main thread
#include "platforms/arm/teensy/coroutine_teensy.impl.hpp"
#elif defined(ARDUINO)
// Generic Arduino (non-ESP32): No-op runtime + null task coroutine
#include "platforms/arduino/coroutine_runtime_arduino.impl.hpp"
#include "platforms/shared/coroutine_null.impl.hpp"
#else
// Unknown platform: null everything
#include "fl/stl/singleton.h"
#include "platforms/coroutine_runtime.h"
#include "platforms/shared/coroutine_null.impl.hpp"

namespace fl {
namespace platforms {
class CoroutineRuntimeNull : public ICoroutineRuntime {
  public:
    void pumpCoroutines(fl::u32) override {}
};
ICoroutineRuntime &ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeNull>::instance();
}
} // namespace platforms
} // namespace fl
#endif
