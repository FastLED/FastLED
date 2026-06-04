// IWYU pragma: private
// ok no namespace fl — this is a platform-dispatch router, not a code file

/// @file platforms/watchdog.impl.cpp.hpp
/// @brief Single dispatcher for fl::Watchdog platform implementations.
///
/// Follows the FastLED `.impl.cpp.hpp` convention (see agents/docs/cpp-standards.md
/// "Naming convention (future standard)"): one file per component, lives in
/// `src/platforms/` root, `#include`d from exactly one translation unit, selects
/// the platform fragment via `is_*.h` macros and falls back to `_noop.hpp`.
///
/// **Phase 1 platforms:**
///   - Host / stub  → `platforms/stub/watchdog_stub.impl.hpp` (real thread timer)
///   - WASM         → `platforms/wasm/watchdog_wasm.impl.hpp` (emscripten timer)
///   - ESP32 family → `platforms/esp/32/watchdog_esp32.impl.hpp` (wraps existing
///                    fl::watchdog_setup() / ESP-IDF TWDT)
///   - All others   → `platforms/shared/watchdog_noop.hpp`
///
/// Per-platform hardware impls for Teensy, RP, nRF52, STM32, SAMD, AVR,
/// Apollo3, MGM240 are tracked as Phase 2..5 in FastLED#2731.

// Platform detection headers — only include those we dispatch on.
#include "platforms/is_platform.h"
#include "platforms/esp/is_esp.h"
#include "platforms/wasm/is_wasm.h"

#if defined(FL_IS_WASM)
    #include "platforms/wasm/watchdog_wasm.impl.hpp"
#elif defined(FASTLED_STUB_IMPL) || defined(FL_IS_STUB)
    #include "platforms/stub/watchdog_stub.impl.hpp"
#elif defined(FL_IS_ESP32)
    #include "platforms/esp/32/watchdog_esp32.impl.hpp"
#else
    // Fallback: no real or emulated WDT available — all methods are no-ops.
    // Per-platform hardware impls will be added incrementally in #2731 follow-ups.
    #include "platforms/shared/watchdog_noop.hpp"
#endif
