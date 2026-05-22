#pragma once

/// @file yield_gate.hpp
/// @brief Smart yield helper for FastLED ESP32 driver polling loops.
///
/// When a driver polls for DMA / peripheral completion it must yield so other
/// FreeRTOS tasks can run. The cheap choice — `taskYIELD()` — only re-runs the
/// scheduler against equal-or-higher-priority Ready tasks; lower-priority
/// network tasks (lwIP / WiFi / BT) never get CPU. The traditional fix is
/// `vTaskDelay(1)`, which Blocks the caller for at least one FreeRTOS tick
/// and lets the scheduler run anything Ready, including lower-priority tasks.
///
/// `vTaskDelay(1)` blocks for between 0 and 1 full tick (depending on phase
/// relative to the next tick interrupt) plus the requested 1 tick — so 1 to 2
/// ticks of actual wall-clock delay on a system with a 1 kHz tick rate. On a
/// 60 FPS render budget that's ~6–12 % of frame time per yield. Paying that
/// when the binary has no network stack linked at all — or when WiFi/BT are
/// linked but inactive — is wasted overhead.
///
/// `fl::platforms::esp_smart_yield()` gates the deep yield on a cached check
/// of whether any network stack is *currently active*:
///
///   - Link-time gate: weak symbols on `esp_wifi_get_mode` and
///     `esp_bt_controller_get_status`. If neither is linked, the check
///     short-circuits to `false` at link time and the deep-yield branch is
///     dead code.
///   - Runtime gate: when symbols are linked, the active-mode check is
///     cached at 1 Hz inside `CoroutineRuntimeEsp32::needsDeepYield()`, so
///     hot-path cost is a singleton lookup plus a compare (first call after
///     boot pays a network-stack probe).
///   - Yield magnitude: when the gate fires we use `vTaskDelay(1)` — the
///     minimum delay that releases the caller from the Ready set under
///     FreeRTOS. Sub-tick requests degenerate to `taskYIELD()` and do not
///     let lower-priority tasks run; that's the failure mode this helper
///     exists to avoid.
///
/// Refs: https://github.com/FastLED/FastLED/issues/2493 (latency analysis),
/// https://github.com/FastLED/FastLED/issues/2254 (the original WiFi
/// starvation case that motivated the round-up-to-one-tick rule).

#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include "platforms/coroutine_runtime.h"
#include "fl/stl/noexcept.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/task.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {
namespace platforms {

/// @brief Yield to the FreeRTOS scheduler with network-stack awareness.
///
/// When a network stack (WiFi / BT) is active, blocks for at least one
/// FreeRTOS tick so lower-priority network tasks can drain. Otherwise just
/// `taskYIELD()`, which is sub-microsecond.
inline void esp_smart_yield() FL_NOEXCEPT {
    if (fl::platforms::ICoroutineRuntime::instance().needsDeepYield()) {
        // Minimum tick-granularity delay that releases the caller from the
        // Ready set, allowing the scheduler to dispatch lower-priority tasks
        // (lwIP / WiFi / BT). Sub-tick requests degenerate to taskYIELD()
        // under FreeRTOS, which is exactly what this branch avoids.
        vTaskDelay(1);
    } else {
        taskYIELD();
    }
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_ESP32
