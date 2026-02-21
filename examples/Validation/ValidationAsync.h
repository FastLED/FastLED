// examples/Validation/ValidationAsync.h
//
// Async task setup for JSON-RPC processing in validation sketch.
// Runs RPC polling on FastLED's task scheduler for non-blocking operation.

#pragma once

#include "ValidationRemote.h"
#include "fl/task.h"
#include "fl/async.h"

namespace validation {

/// @brief Setup async task for JSON-RPC processing
/// @param remote_control Reference to RemoteControl singleton
/// @param interval_ms Task interval in milliseconds (default: 10ms)
/// @return Task handle (auto-registered with scheduler)
///
/// This function creates an async task that polls the RPC system at regular intervals,
/// allowing RPC commands to be processed without blocking the main loop.
///
/// The 10ms default interval balances:
/// - Responsiveness: 115200 baud ≈ 100 bytes in 10ms
/// - CPU overhead: Minimal impact on LED peripheral operations
///
/// The task is automatically registered with the scheduler via .then() and will
/// run until the program exits.
///
/// THREAD SAFETY:
/// - Safe to capture remote_control by reference (singleton lifetime)
/// - ESP32 Arduino runs on single core - task switching is atomic
/// - No additional synchronization needed
inline fl::task setupRpcAsyncTask(ValidationRemoteControl& remote_control, int interval_ms = 10) {
    return fl::task::every_ms(interval_ms, FL_TRACE)
        .then([&remote_control]() {
            uint32_t current_time = millis();
            remote_control.tick(current_time);  // Calls Remote::update() which does pull + tick + push
        });
}

/// @brief On FL_IS_STUB: register a one-shot async task that drives validation
///
/// On the stub (native/host) platform, this registers an async task that:
/// 1. Discovers available drivers
/// 2. Tests each driver with validateChipsetTiming()
/// 3. Collects results and exits 0 (all passed) or 1 (failure/no tests)
///
/// On all other platforms (ESP32, etc.), this is a no-op.
///
/// @param remote Reference to RemoteControl singleton (unused on non-stub)
/// @param state Shared validation state (contains driver list, pins, RX channel)
inline void maybeRegisterStubAutorun(
        ValidationRemoteControl& /*remote*/,
        fl::shared_ptr<ValidationState> state) {
#ifdef FL_IS_STUB
    // Register a task that runs on the next async_run() cycle (during loop())
    // Note: every_ms(0) fires immediately; after_frame() requires frame-task
    // dispatch which isn't wired up in the stub example runner.
    fl::task::every_ms(0, FL_TRACE).then([state]() {
        if (!state || state->drivers_available.empty()) {
            FL_ERROR("[STUB CLIENT] No drivers discovered — validation cannot run");
            exit(1);
        }

        FL_PRINT("\n[STUB CLIENT] ============================================");
        FL_PRINT("[STUB CLIENT] Simulated host client — running validation");
        FL_PRINT("[STUB CLIENT] Drivers: " << state->drivers_available.size());
        FL_PRINT("[STUB CLIENT] ============================================");

        // WS2812B-V5 timing (same as the Python client default)
        fl::NamedTimingConfig timing_cfg(
            fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        // Static LED storage — span must remain valid for the entire call
        static CRGB stub_leds[10];
        const int num_leds = 10;

        // ChannelConfig stores timing by value internally, so passing a local ref is fine
        fl::ChannelConfig stub_tx_cfg(
            state->pin_tx,
            timing_cfg.timing,
            fl::span<CRGB>(stub_leds, num_leds),
            RGB);

        int grand_total = 0, grand_passed = 0;

        for (fl::size di = 0; di < state->drivers_available.size(); di++) {
            const auto& drv = state->drivers_available[di];
            FL_PRINT("\n[STUB CLIENT] Driver: " << drv.name.c_str());

            if (!FastLED.setExclusiveDriver(drv.name.c_str())) {
                FL_ERROR("[STUB CLIENT] Failed to activate driver: " << drv.name.c_str());
                grand_total++;  // count as a failure
                continue;
            }

            // ValidationConfig holds timing by reference — timing_cfg is in scope
            fl::ValidationConfig vcfg(
                timing_cfg.timing,
                timing_cfg.name,
                fl::span<fl::ChannelConfig>(&stub_tx_cfg, 1),
                drv.name.c_str(),
                state->rx_channel,
                state->rx_buffer,
                num_leds,
                fl::RxDeviceType::RMT);

            int driver_total = 0, driver_passed = 0;
            validateChipsetTiming(vcfg, driver_total, driver_passed);

            FL_PRINT("[STUB CLIENT] " << drv.name.c_str()
                     << ": " << driver_passed << "/" << driver_total << " passed");

            grand_total  += driver_total;
            grand_passed += driver_passed;
        }

        FL_PRINT("\n[STUB CLIENT] ============================================");
        FL_PRINT("[STUB CLIENT] TOTAL: " << grand_passed << "/" << grand_total);

        if (grand_total == 0) {
            FL_ERROR("[STUB CLIENT] No tests ran — exiting 1");
            exit(1);
        } else if (grand_passed == grand_total) {
            FL_PRINT("[STUB CLIENT] ALL TESTS PASSED ✓ — exiting 0");
            exit(0);
        } else {
            FL_ERROR("[STUB CLIENT] " << (grand_total - grand_passed)
                     << " TESTS FAILED — exiting 1");
            exit(1);
        }
    });
#endif
}

} // namespace validation
