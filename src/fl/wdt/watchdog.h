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
#include "fl/stl/string_view.h"        // for resetCauseName() return type

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

/// @brief Return a static-lifetime name for `c` — zero-cost, no allocation.
///
/// String literal lives in flash on embedded platforms. Safe to call from any
/// context (ISR, panic handler, before setup() finishes). Backed by
/// `fl::string_view` so the result has no destructor and can be passed by
/// value freely.
inline fl::string_view resetCauseName(ResetCause c) FL_NOEXCEPT {
    switch (c) {
        case ResetCause::POWER_ON:     return fl::string_view("POWER_ON");
        case ResetCause::BROWNOUT:     return fl::string_view("BROWNOUT");
        case ResetCause::EXTERNAL_PIN: return fl::string_view("EXTERNAL_PIN");
        case ResetCause::WATCHDOG:     return fl::string_view("WATCHDOG");
        case ResetCause::SOFTWARE:     return fl::string_view("SOFTWARE");
        case ResetCause::LOCKUP:       return fl::string_view("LOCKUP");
        case ResetCause::DEBUGGER:     return fl::string_view("DEBUGGER");
        case ResetCause::PANIC:        return fl::string_view("PANIC");
        case ResetCause::UNKNOWN:
        default:                       return fl::string_view("UNKNOWN");
    }
}

/// @brief Detailed reset information bundling the normalized cause with a
/// platform-specific subcause id and the raw cause-register value.
///
/// `subcauseId` and `rawRegister` are zero on platforms that don't expose
/// them; on Teensy 4 the subcause distinguishes WDOG3 from WDOG1/2 etc.,
/// and rawRegister holds the captured `SRC_SRSR` bits.
struct ResetInfo {
    ResetCause cause;          ///< Normalized cross-platform enum
    fl::u8     subcauseId;     ///< Platform-specific subcause id (0 = none)
    fl::u32    rawRegister;    ///< Raw value of the platform's reset-cause register

    /// Cause name (zero-cost, static string_view).
    fl::string_view causeName() const FL_NOEXCEPT { return resetCauseName(cause); }

    /// Platform-specific subcause name (zero-cost, static string_view).
    /// Returns an empty view if `subcauseId == 0` or the platform doesn't
    /// define names. Per-platform impls extend this via a strong override.
    fl::string_view subcauseName() const FL_NOEXCEPT;

    /// Write a single-line human-readable description into the caller's
    /// buffer. Returns the number of bytes written (NOT including any
    /// trailing NUL, though a NUL is appended if there is room). Truncates
    /// safely if `out` is too small. Never allocates. Format:
    ///
    ///   "<cause>"                          if subcauseId == 0
    ///   "<cause> (<sub>)"                  if subcauseId != 0 && !verbose
    ///   "<cause> (<sub>) raw=0x<hex>"      if verbose
    fl::size describe(fl::span<char> out, bool verbose = false) const FL_NOEXCEPT;
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

    /// @brief Detailed reset information including platform raw register +
    /// subcause id. Default implementation returns {lastResetCause(), 0, 0};
    /// platforms that capture the raw register override with a strong
    /// definition in their `.impl.hpp`.
    ResetInfo lastResetInfo() const FL_NOEXCEPT;

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

/// @brief RAII watchdog guard for the canonical `loop()`-top use case.
///
/// First construction (per-process) lazily arms the watchdog with the supplied
/// timeout (default 15 000 ms), prints any prior-boot reset/crash diagnostic
/// through `fl::println`, and pauses for 3 s on a crash so the developer can
/// read the message before `loop()` repaints serial output. Construction
/// feeds the watchdog immediately so the current loop iteration starts with
/// a fresh deadline; destruction feeds it again so the next iteration also
/// starts fresh.
///
/// **Single-instance rule.** Only one `ScopedWatchdog` should be alive at a
/// time. The class detects nested / overlapping live instances at runtime
/// and prints a one-shot warning via `fl::println` (the WDT is still fed in
/// both ctors and dtors so the program keeps running). Two guards on the
/// same source line are a compile error (the `FL_WATCHDOG_AUTO` macro
/// stamps the local variable name from `__LINE__`, so a duplicate would
/// collide).
///
/// Prefer the `FL_WATCHDOG_AUTO(...)` macro below — it stamps a unique
/// stack variable name and is the canonical entry point.
class ScopedWatchdog {
public:
    /// Default construct with the library default timeout (15 000 ms).
    ScopedWatchdog() FL_NOEXCEPT : ScopedWatchdog(15000u) {}

    /// Construct with an explicit timeout. Same semantics as the default
    /// constructor — first-call init, feed on construction.
    explicit ScopedWatchdog(fl::u32 timeout_ms) FL_NOEXCEPT;

    /// Feed the watchdog at end-of-scope so the next `loop()` iteration has
    /// a clean deadline window.
    ~ScopedWatchdog() FL_NOEXCEPT;

    /// Observability: number of simultaneously-alive ScopedWatchdog instances.
    /// Used by tests + diagnostics. A value > 1 means a nested-guard
    /// programmer error was hit; the WDT is still being fed.
    static int activeScopeCount() FL_NOEXCEPT;

    ScopedWatchdog(const ScopedWatchdog&)            FL_NOEXCEPT = delete;
    ScopedWatchdog& operator=(const ScopedWatchdog&) FL_NOEXCEPT = delete;
};

} // namespace fl

// ============================================================================
// FL_WATCHDOG_AUTO(...) — the canonical zero-setup loop()-top guard.
//
// Variadic so the same macro covers both the no-argument default (15 000 ms)
// and the explicit-timeout form:
//
//   void loop() {
//       FL_WATCHDOG_AUTO();      // 15 000 ms default
//       doWork();
//   }
//
//   void loop() {
//       FL_WATCHDOG_AUTO(5000);  // 5-second timeout
//       doWork();
//   }
//
// "AUTO" because this macro does more than just feed: it lazily arms the WDT
// on first call, prints the prior-boot reset/crash diagnostic, pauses 3 s on
// a crash so the developer can read the message, and feeds the WDT on both
// construction AND destruction. Single source-line use; the macro stamps the
// local variable name from `__LINE__` so two `FL_WATCHDOG_AUTO()` on the same
// line are a compile error. Two on different lines but simultaneously alive
// (nested scopes) trigger a one-shot runtime warning — see ScopedWatchdog.
//
// Future variants stay grouped under the `FL_WATCHDOG_*` prefix:
//   FL_WATCHDOG_AUTO_QUIET / FL_WATCHDOG_AUTO_VERBOSE — log-level variants
//   FL_WATCHDOG_MANUAL                                — non-RAII setup helper
// ============================================================================

#define FL_WATCHDOG__CONCAT_INNER(a, b) a##b
#define FL_WATCHDOG__CONCAT(a, b)       FL_WATCHDOG__CONCAT_INNER(a, b)
#define FL_WATCHDOG_AUTO(...)                                                  \
    ::fl::ScopedWatchdog FL_WATCHDOG__CONCAT(_fl_wdt_, __LINE__) { __VA_ARGS__ }

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
