// src/platforms/esp/32/watchdog_esp32_idf5.hpp
//
// ESP32 Watchdog Timer Implementation - ESP-IDF v5.x
//
// IDF v5.x-specific implementation using official shutdown handler API.
// This version uses esp_register_shutdown_handler() since ESP-IDF v5.0+
// made panic handler functions private and no longer supports weak symbol override.

#pragma once

#include "watchdog_esp32.h"

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

#include "fl/dbg.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platforms/esp/esp_version.h"

// USB Serial JTAG registers - only available on ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || \
    CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
    #include "soc/usb_serial_jtag_reg.h"
    #define HAS_USB_SERIAL_JTAG 1
#else
    #define HAS_USB_SERIAL_JTAG 0
#endif

// ROM delay function - works in any context including panic handlers
#include "esp_rom_sys.h"

namespace fl {
namespace detail {

// Callback storage - shared between watchdog setup and reset handlers
static watchdog_callback_t s_user_callback = nullptr;
static void* s_user_data = nullptr;

// USB disconnect delay for Windows compatibility (150ms minimum)
constexpr uint32_t USB_DISCONNECT_DELAY_US = 150000;

} // namespace detail

// ============================================================================
// USB Disconnect Logic
// ============================================================================

namespace {

// Performs hardware-level USB disconnect sequence
void disconnect_usb_hardware() {
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
void invoke_user_callback() {
    if (detail::s_user_callback != nullptr) {
        detail::s_user_callback(detail::s_user_data);
    }
}

// Common reset handler logic
void handle_system_reset(const char* handler_name) {
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
static void watchdog_shutdown_handler_v5(void) {
    handle_system_reset("SHUTDOWN FastLED idfv5");
}
} // anonymous namespace


// ============================================================================
// Watchdog Configuration
// ============================================================================

namespace {

// Deinitializes existing watchdog if scheduler is running
void deinit_existing_watchdog() {
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        esp_task_wdt_deinit();
    }
}

// Initializes watchdog with specified timeout
bool init_task_watchdog(uint32_t timeout_ms) {
    esp_task_wdt_config_t config = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = (1 << 0),  // Monitor idle task on core 0 (main loop)
        .trigger_panic = true         // Trigger panic and reset on timeout
    };

    esp_err_t err = esp_task_wdt_init(&config);
    if (err != ESP_OK) {
        FL_DBG("[WATCHDOG] Failed to initialize (error: " << err << ")");
        return false;
    }

    return true;
}

// Logs watchdog configuration status
void log_watchdog_status(uint32_t timeout_ms, watchdog_callback_t callback) {
    FL_DBG("[WATCHDOG] ✓ " << timeout_ms << "ms watchdog active with reset on timeout");
    if (callback != nullptr) {
        FL_DBG("[WATCHDOG] ℹ️  User callback registered");
    }
    FL_DBG("[WATCHDOG] ℹ️  Automatically monitors loop() execution - no manual feeding needed");
}


} // anonymous namespace

void watchdog_setup(uint32_t timeout_ms,
                    watchdog_callback_t callback,
                    void* user_data) {
    FL_DBG("\n[WATCHDOG] Configuring ESP32 custom " << timeout_ms << "ms watchdog (IDF v5.x)");

    // Store callback for reset handlers
    detail::s_user_callback = callback;
    detail::s_user_data = user_data;

    // Register shutdown handler for safe USB disconnect before reset
    // This is the proper ESP-IDF v5.0+ API for pre-reset cleanup
    esp_register_shutdown_handler(watchdog_shutdown_handler_v5);

    deinit_existing_watchdog();

    if (!init_task_watchdog(timeout_ms)) {
        return;
    }

    log_watchdog_status(timeout_ms, callback);
}

} // namespace fl

#endif // ESP32
