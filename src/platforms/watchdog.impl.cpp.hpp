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
/// Platform coverage:
///   - Host / stub      → `platforms/stub/watchdog_stub.impl.hpp` (thread timer)
///   - WASM             → `platforms/wasm/watchdog_wasm.impl.hpp` (routes to noop)
///   - ESP32 family     → `platforms/esp/32/watchdog_esp32.impl.hpp` (wraps fl::watchdog_setup)
///   - Teensy 4 (T4.x)  → `platforms/arm/mxrt1062/watchdog_mxrt1062.impl.hpp` (WDT_T4<WDT3>)
///   - Teensy 3 (T3.x)  → `platforms/arm/k20/watchdog_k20.impl.hpp` (direct WDOG regs)
///   - RP2040 / RP2350  → `platforms/arm/rp/watchdog_rp.impl.hpp` (Pico SDK)
///   - nRF52            → `platforms/arm/nrf52/watchdog_nrf52.impl.hpp` (NRF_WDT)
///   - STM32            → `platforms/arm/stm32/watchdog_stm32.impl.hpp` (IWDG)
///   - SAMD21 / SAMD51  → `platforms/arm/samd/watchdog_samd.impl.hpp` (SleepyDog)
///   - Apollo3          → `platforms/apollo3/watchdog_apollo3.impl.hpp` (am_hal_wdt)
///   - MGM240           → `platforms/arm/mgm240/watchdog_mgm240.impl.hpp` (em_wdog)
///   - AVR              → `platforms/avr/watchdog_avr.impl.hpp` (avr/wdt.h + .init3 fix)
///   - Other            → `platforms/shared/watchdog_noop.hpp` (no-op fallback)
///
/// **Library availability guards:** Some platform impls depend on third-party
/// Arduino libraries (Watchdog_t4, IWatchdog, Adafruit_SleepyDog, am_mcu_apollo,
/// em_wdog) that may not be installed in every build configuration. We gate
/// each such route with `__has_include(<header>)` so that when the optional
/// library is missing the dispatcher falls through to the noop fallback rather
/// than breaking the compile. User code that calls the watchdog API still
/// works (as a no-op) — opt back into hardware behavior by installing the
/// matching library.

// Platform detection headers
#include "platforms/is_platform.h"
#include "platforms/esp/is_esp.h"
#include "platforms/wasm/is_wasm.h"
#include "fl/stl/has_include.h"

#if defined(FL_IS_WASM)
    #include "platforms/wasm/watchdog_wasm.impl.hpp"
#elif defined(FASTLED_STUB_IMPL) || defined(FL_IS_STUB)
    #include "platforms/stub/watchdog_stub.impl.hpp"
#elif defined(FL_IS_ESP32)
    #include "platforms/esp/32/watchdog_esp32.impl.hpp"
#elif defined(FL_IS_TEENSY_4X) && FL_HAS_INCLUDE(<imxrt.h>)
    #include "platforms/arm/mxrt1062/watchdog_mxrt1062.impl.hpp"
#elif (defined(FL_IS_TEENSY_3X) || defined(FL_IS_TEENSY_LC)) && FL_HAS_INCLUDE(<kinetis.h>)
    #include "platforms/arm/k20/watchdog_k20.impl.hpp"
#elif (defined(FL_IS_RP2040) || defined(FL_IS_RP2350)) && FL_HAS_INCLUDE(<hardware/watchdog.h>)
    #include "platforms/arm/rp/watchdog_rp.impl.hpp"
#elif defined(FL_IS_NRF52) && FL_HAS_INCLUDE(<nrf.h>)
    #include "platforms/arm/nrf52/watchdog_nrf52.impl.hpp"
#elif defined(FL_IS_STM32) && FL_HAS_INCLUDE(<IWatchdog.h>)
    #include "platforms/arm/stm32/watchdog_stm32.impl.hpp"
#elif (defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)) && FL_HAS_INCLUDE(<Adafruit_SleepyDog.h>)
    #include "platforms/arm/samd/watchdog_samd.impl.hpp"
#elif defined(FL_IS_APOLLO3) && FL_HAS_INCLUDE(<am_mcu_apollo.h>)
    #include "platforms/apollo3/watchdog_apollo3.impl.hpp"
#elif defined(FL_IS_SILABS_MGM240) && FL_HAS_INCLUDE(<em_wdog.h>)
    #include "platforms/arm/mgm240/watchdog_mgm240.impl.hpp"
#elif defined(FL_IS_AVR) && FL_HAS_INCLUDE(<avr/wdt.h>)
    #include "platforms/avr/watchdog_avr.impl.hpp"
#else
    // Fallback: no real or emulated WDT available — all methods are no-ops.
    // Reached when the platform isn't supported OR when the optional Arduino
    // library backing this platform isn't installed (see header note above).
    // The api still compiles and runs on every supported MCU.
    #include "platforms/shared/watchdog_noop.hpp"
#endif

// Platform-agnostic helpers (ResetInfo::describe(), ScopedWatchdog ctor/dtor,
// print/delay sinks for the lazy first-init path) are emitted by
// `src/fl/wdt/_build.cpp.hpp` — included from the regular fl/ unity build,
// not from here. Including `.cpp.hpp` files outside the unity build is a
// hard ban per agents/docs/cpp-standards.md.
