#pragma once

/// @file yield_gate.hpp
/// @brief Smart yield helper for FastLED ESP32 driver polling loops.
///
/// When a driver polls for DMA / peripheral completion it must yield so other
/// FreeRTOS tasks can run. The cheap choice — `taskYIELD()` — only re-runs the
/// scheduler against equal-or-higher-priority Ready tasks; lower-priority
/// network tasks (lwIP / WiFi / BT) never get CPU. The traditional fix is
/// `vTaskDelay(1)`, which Blocks the caller for exactly one FreeRTOS tick
/// and lets the scheduler run anything Ready, including lower-priority tasks.
///
/// `vTaskDelay(1)` costs one full tick (~1 ms on the default 1 kHz tick
/// rate). On a 60 FPS render budget that's ~6 % of frame time. Paying that
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
///     per-call cost is a singleton lookup plus one compare.
///   - Yield magnitude: when the gate fires we use `vTaskDelay(1)` — exactly
///     one tick by the FreeRTOS contract, regardless of the configured tick
///     rate. Sub-tick delays are not meaningful in FreeRTOS terms.
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
/// When a network stack (WiFi / BT) is active, blocks for exactly one
/// FreeRTOS tick so lower-priority network tasks can drain. Otherwise just
/// `taskYIELD()`, which is sub-microsecond.
inline void esp_smart_yield() FL_NOEXCEPT {
    if (fl::ICoroutineRuntime::instance().needsDeepYield()) {
        // Exactly one tick — the smallest interval that releases the caller
        // from the Ready set and lets the scheduler dispatch lower-priority
        // tasks. Sub-tick requests round to taskYIELD() under FreeRTOS, which
        // is what this branch is trying to avoid.
        vTaskDelay(1);
    } else {
        taskYIELD();
    }
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_ESP32
