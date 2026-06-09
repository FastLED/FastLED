// src/platforms/esp/32/watchdog_esp32_idf5.hpp
//
// ESP32 Watchdog Timer Implementation - ESP-IDF v5.x
//
// IDF v5.x-specific implementation using official shutdown handler API.
// This version uses esp_register_shutdown_handler() since ESP-IDF v5.0+
// made panic handler functions private and no longer supports weak symbol override.

#pragma once

// IWYU pragma: private

#include "platforms/esp/32/watchdog_esp32.h"

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

#include "fl/log/log.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/task.h"
// IWYU pragma: end_keep
#include "platforms/esp/esp_version.h"

// USB Serial JTAG registers - only available on ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2
#if defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    // IWYU pragma: begin_keep
    #include "soc/usb_serial_jtag_reg.h"
    // IWYU pragma: end_keep
    #define HAS_USB_SERIAL_JTAG 1
#else
    #define HAS_USB_SERIAL_JTAG 0
#endif

// ROM delay function - works in any context including panic handlers
#include "esp_rom_sys.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

// Callback storage - shared between watchdog setup and reset handlers
static watchdog_callback_t s_user_callback = nullptr;
static void* s_user_data = nullptr;

// USB disconnect delay for Windows compatibility (150ms minimum)
constexpr u32 USB_DISCONNECT_DELAY_US = 150000;

} // namespace detail

// ============================================================================
// USB Disconnect Logic
// ============================================================================

namespace {

// Performs hardware-level USB disconnect sequence
void disconnect_usb_hardware() FL_NOEXCEPT {
#if HAS_USB_SERIAL_JTAG
    // Clear D+ pullup to signal USB disconnect
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);

    // Pull D+ low to ensure Windows detects disconnect
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);

    // Wait for Windows to detect disconnect
    // Use ROM-based delay that works in panic/shutdown context
    // (fl::delayMicroseconds uses Arduino delay which may not work in panic handlers)
    esp_rom_delay_us(detail::USB_DISCONNECT_DELAY_US);
#endif
}

// Invokes user callback if registered
void invoke_user_callback() FL_NOEXCEPT {
    if (detail::s_user_callback != nullptr) {
        detail::s_user_callback(detail::s_user_data);
    }
}

// Common reset handler logic
void handle_system_reset(const char* handler_name) FL_NOEXCEPT {
    invoke_user_callback();

    FL_DBG("\n[" << handler_name << "] System reset detected - performing safe USB disconnect");

    disconnect_usb_hardware();

#if HAS_USB_SERIAL_JTAG
    FL_DBG("[" << handler_name << "] ✓ USB disconnected - proceeding with reset");
#else
    FL_DBG("[" << handler_name << "] No USB Serial JTAG hardware - using default reset behavior");
#endif
}

} // anonymous namespace

// ESP-IDF v5.0+ shutdown handler
// Uses official shutdown handler API (esp_register_shutdown_handler)
// See: esp-idf/components/esp_system/include/esp_system.h
// IMPORTANT: Defined in fl namespace scope so it can be registered with ESP-IDF
// and can access helper functions from the anonymous namespace above
namespace {
static void watchdog_shutdown_handler_v5(void) FL_NOEXCEPT {
    handle_system_reset("SHUTDOWN FastLED idfv5");
}
} // anonymous namespace


// ============================================================================
// Watchdog Configuration
// ============================================================================

namespace {

// Deinitializes existing watchdog if scheduler is running.
// Returns true on successful deinit (or if no deinit is needed because the
// scheduler isn't running), false if `esp_task_wdt_deinit()` reported an
// error (e.g., other tasks still subscribed, already deinitialized).
bool deinit_existing_watchdog() FL_NOEXCEPT {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        // Nothing to tear down — the TWDT cannot have been armed.
        return true;
    }
    esp_err_t err = esp_task_wdt_deinit();
    return err == ESP_OK;
}

// Initializes watchdog with specified timeout
bool init_task_watchdog(u32 timeout_ms) FL_NOEXCEPT {
    esp_task_wdt_config_t config = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = (1 << 0),  // Also monitor CPU starvation via the idle task.
        .trigger_panic = true         // Trigger panic and reset on timeout
    };

    esp_err_t err = esp_task_wdt_init(&config);
    if (err == ESP_ERR_INVALID_STATE) {
        err = esp_task_wdt_reconfigure(&config);
    }
    if (err != ESP_OK) {
        FL_DBG("[WATCHDOG] Failed to initialize (error: " << err << ")");
        return false;
    }

    return true;
}

// Logs watchdog configuration status
void log_watchdog_status(u32 timeout_ms, watchdog_callback_t callback) FL_NOEXCEPT {
    FL_DBG("[WATCHDOG] ✓ " << timeout_ms << "ms watchdog active with reset on timeout");
    if (callback != nullptr) {
        FL_DBG("[WATCHDOG] ℹ️  User callback registered");
    }
    FL_DBG("[WATCHDOG] Monitors the loop task plus idle-task CPU starvation");
}


} // anonymous namespace

void watchdog_setup(u32 timeout_ms,
                    watchdog_callback_t callback,
                    void* user_data) FL_NOEXCEPT {
    FL_DBG("\n[WATCHDOG] Configuring ESP32 custom " << timeout_ms << "ms watchdog (IDF v5.x)");

    // Store callback for reset handlers
    detail::s_user_callback = callback;
    detail::s_user_data = user_data;

    // Register shutdown handler for safe USB disconnect before reset
    // This is the proper ESP-IDF v5.0+ API for pre-reset cleanup
    esp_register_shutdown_handler(watchdog_shutdown_handler_v5);

    // Best-effort deinit before re-init; failure here is non-fatal because
    // we're about to overwrite the configuration anyway.
    (void)deinit_existing_watchdog();

    if (!init_task_watchdog(timeout_ms)) {
        return;
    }

    log_watchdog_status(timeout_ms, callback);
}

bool watchdog_disable() FL_NOEXCEPT {
    if (!deinit_existing_watchdog()) {
        // TWDT is still active — leave the user callback bound so it still
        // fires on timeout.
        return false;
    }
    detail::s_user_callback = nullptr;
    detail::s_user_data = nullptr;
    return true;
}

} // namespace fl

#endif // FL_IS_ESP32
