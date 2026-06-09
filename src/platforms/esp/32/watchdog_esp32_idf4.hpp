// src/platforms/esp/32/watchdog_esp32_idf4.hpp
//
// ESP32 Watchdog Timer Implementation - ESP-IDF v4.x
//
// IDF v4.x-specific implementation using weak symbol override for panic handler.
// This version overrides esp_panic_handler_reconfigure_wdts() to intercept
// watchdog-triggered panics and perform safe USB disconnect before reset.

#pragma once

// IWYU pragma: private

#include "platforms/esp/32/watchdog_esp32.h"

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

#include "fl/stl/compiler_control.h"
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
// esp_rom_sys.h is only available in ESP-IDF v4.0+
#if ESP_IDF_VERSION_4_OR_HIGHER
    #include "esp_rom_sys.h"
#else
    // ESP-IDF v3.x: ROM functions are in rom/ets_sys.h
    // IWYU pragma: begin_keep
    #include "rom/ets_sys.h"
#include "fl/stl/noexcept.h"
    // IWYU pragma: end_keep
#endif

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

namespace detail {

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
    esp_rom_delay_us(USB_DISCONNECT_DELAY_US);
#endif
}

// Invokes user callback if registered
void invoke_user_callback() FL_NOEXCEPT {
    if (s_user_callback != nullptr) {
        s_user_callback(s_user_data);
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

} // namespace detail

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
        return true;
    }
    esp_err_t err = esp_task_wdt_deinit();
    return err == ESP_OK;
}

// Initializes watchdog with specified timeout
bool init_task_watchdog(u32 timeout_ms) FL_NOEXCEPT {
    // ESP-IDF v4.x uses simple parameters: esp_task_wdt_init(timeout_seconds, panic)
    // Convert milliseconds to seconds (ESP-IDF v4.x uses seconds, not milliseconds)
    u32 timeout_s = (timeout_ms + 999) / 1000;  // Round up to nearest second

    esp_err_t err = esp_task_wdt_init(timeout_s, true);  // true = trigger panic on timeout
    if (err == ESP_ERR_INVALID_STATE) {
        err = ESP_OK;  // Already armed; keep the existing IDF4 timeout.
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
    FL_DBG("\n[WATCHDOG] Configuring ESP32 custom " << timeout_ms << "ms watchdog (IDF v4.x)");

    // Store callback for reset handlers
    detail::s_user_callback = callback;
    detail::s_user_data = user_data;

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
        return false;
    }
    detail::s_user_callback = nullptr;
    detail::s_user_data = nullptr;
    return true;
}

} // namespace fl

// ============================================================================
// IDF v4.x Panic Handler (Weak Symbol Override)
// ============================================================================

// ESP-IDF v4.x panic handler override
// Overrides weak symbol in esp-idf/components/esp_system/panic.c
// IMPORTANT: Must qualify with fl::detail:: to access function in detail namespace
// NOTE: Marked as weak to avoid multiple definition conflicts with ESP-IDF
extern "C" FL_LINK_WEAK void esp_panic_handler_reconfigure_wdts(void) {
    fl::detail::handle_system_reset("PANIC FastLED idfv4");
}

#endif // FL_IS_ESP32
