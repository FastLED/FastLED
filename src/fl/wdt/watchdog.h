#pragma once

/// @file fl/wdt/watchdog.h
/// @brief Unified cross-platform Watchdog Timer API for FastLED.
///
/// Accessed via the FastLED god-object: `FastLED.watchdog()`.
///
/// See FastLED#2731 for the full design rationale and per-platform capability matrix.
///
/// **Three tiers:**
///   - Tier 0 — universal (every supported platform, incl. AVR)
///   - Tier 1 — pre-reset callback, pause-on-debug, flash crash log, bootloader reboot
///   - Tier 2 — window mode, decoded CrashReport
///
/// **Naming:** spelled-out (`watchdog`, not `wdt`). Macros: `FL_WATCHDOG_*`, not `FL_WDT_*`.
///
/// **Usage:**
/// @code
/// void setup() {
///     FastLED.watchdog().begin(15000);
///     if (FastLED.watchdog().isInSafeMode()) {
///         // skip LED init, wait for new firmware
///         while (true) {}
///     }
/// }
/// void loop() {
///     doWork();
///     FastLED.watchdog().feed();          // after the dangerous work
///     FastLED.watchdog().markCleanShutdown();
/// }
/// @endcode

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/compiler_control.h"   // FL_NORETURN
#include "fl/stl/span.h"
#include "fl/stl/function.h"

namespace fl {

/// @brief Cause of the previous boot. Call once early in setup().
enum class ResetCause : fl::u8 {
    UNKNOWN = 0,
    POWER_ON,
    BROWNOUT,
    EXTERNAL_PIN,
    WATCHDOG,
    SOFTWARE,
    LOCKUP,
    DEBUGGER,
    PANIC,
};

/// @brief ISR-safe C function pointer callback for `onTimeout()`.
using WatchdogTimeoutCallback = void(*)(void* user_data);

/// @brief Decoded crash report (Tier 2).
struct WatchdogCrashReport {
    bool        valid;
    fl::u32     pc;
    fl::u32     lr;
    fl::u32     psr;
    fl::u32     fault_status;
    fl::u32     fault_address;
    const char* fault_type;
};

/// @brief Unified cross-platform watchdog interface.
///
/// Singleton accessed through `FastLED.watchdog()` or `fl::Watchdog::instance()`.
class Watchdog {
public:
    // Singleton accessor — DECLARATION ONLY. The function-local `static
    // Watchdog` must live in a `.cpp.hpp` / `.impl.hpp` TU (one definition,
    // one guard variable) so the Teensy 3.x `__cxa_guard` ABI conflict does
    // not pull a guarded singleton path into every TU that includes
    // FastLED.h. See `agents/docs/cpp-standards.md` → "Header rules:
    // function-local statics with non-trivial constructors are forbidden in
    // `src/**/*.h`."
    static Watchdog& instance() FL_NOEXCEPT;

    // ========== Tier 0 — universal ==========

    void       begin(fl::u32 timeout_ms) FL_NOEXCEPT;
    void       feed() FL_NOEXCEPT;
    void       disable() FL_NOEXCEPT;

    ResetCause lastResetCause() const FL_NOEXCEPT;
    bool       lastResetWasWatchdog() const FL_NOEXCEPT;

    fl::u8     persistRead(fl::size idx) const FL_NOEXCEPT;
    void       persistWrite(fl::size idx, fl::u8 v) FL_NOEXCEPT;

    fl::u16    consecutiveCrashCount() const FL_NOEXCEPT;
    void       markCleanShutdown() FL_NOEXCEPT;
    bool       isInSafeMode() const FL_NOEXCEPT;
    fl::u16    safeModeThreshold() const FL_NOEXCEPT;
    void       setSafeModeThreshold(fl::u16 threshold) FL_NOEXCEPT;

    FL_NORETURN void reboot() FL_NOEXCEPT;

    // ========== Tier 1 — most platforms ==========

    bool       onTimeout(WatchdogTimeoutCallback cb, void* user_data = nullptr) FL_NOEXCEPT;
    bool       onTimeout(fl::function<void()> cb) FL_NOEXCEPT;
    bool       setPauseOnDebug(bool pause) FL_NOEXCEPT;
    bool       writeCrashLog(fl::span<const fl::u8> payload) FL_NOEXCEPT;
    fl::size   readCrashLog(fl::span<fl::u8> out) const FL_NOEXCEPT;
    bool       rebootIntoBootloader() FL_NOEXCEPT;

    // ========== Tier 2 — advanced platforms ==========

    bool                setWindow(fl::u32 window_min_ms, fl::u32 window_max_ms) FL_NOEXCEPT;
    bool                hasCrashReport() const FL_NOEXCEPT;
    WatchdogCrashReport readCrashReport() const FL_NOEXCEPT;
    void                clearCrashReport() FL_NOEXCEPT;

private:
    Watchdog() FL_NOEXCEPT = default;
    ~Watchdog() FL_NOEXCEPT = default;
    Watchdog(const Watchdog&) FL_NOEXCEPT = delete;
    Watchdog& operator=(const Watchdog&) FL_NOEXCEPT = delete;

    fl::u16 mSafeModeThreshold = 2;
};

} // namespace fl

// ============================================================================
// FL_WATCHDOG_* capability macros — defined by the per-platform impl.
// The noop fallback defines NONE of the boolean flags and defaults numerics to 0.
// User code may #ifdef on the boolean flags or static_assert on the numerics.
// ============================================================================
//
//   #define FL_WATCHDOG_HAS_HARDWARE          // 1 wherever a real or emulated WDT runs
//   #define FL_WATCHDOG_PERSIST_BYTES N       // small persistent buffer size, >= 8
//   #define FL_WATCHDOG_HAS_PRE_TIMEOUT_IRQ   // pre-reset callback honored at HW level
//   #define FL_WATCHDOG_HAS_WINDOW_MODE       // setWindow() honored
//   #define FL_WATCHDOG_HAS_CRASH_REPORT      // hasCrashReport()/readCrashReport() honored
//   #define FL_WATCHDOG_HAS_BOOTLOADER_REBOOT // rebootIntoBootloader() honored
//   #define FL_WATCHDOG_MAX_TIMEOUT_MS N      // upper bound, in ms
//
// The Tier 0 surface is always callable; Tier 1/2 methods return false on
// platforms whose impl does not honor them, so user code that checks the
// return value works portably.
